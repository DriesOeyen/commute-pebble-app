var locationLat;
var locationLon;
var locationFound = false;
var locationError = false;
var locationOptions = {
	enableHighAccuracy: true,
	maximumAge: 120000,
	timeout: 10000
};

var requestId = -1;
var requestOrig;
var requestDest;
var requestReceived = false;

var responseTypeReady = 0;
var responseTypeLocated = 1;
var responseTypeDirections = 2;
var responseTypeError = 3;
var responseTypeConfigChanged = 4;

var errorTimelineToken = 0;
var errorLocation = 1;
var errorInternetTimeout = 2;
var errorInternetUnavailable = 3;
var errorResponseUnexpected = 4;
var errorResponseNoTrafficData = 5;
var errorResponseAddressIncorrect = 6;
var errorResponseNoRoute = 7;
var errorConfigure = 8;
var errorReconfigure = 9;
//var errorBluetoothDisconnected = 10;
//var errorBluetoothTransmission = 11;

var storageVersionLatest = 1;
var gaeBaseUrl = "https://commute-pebble.appspot.com";
/* Keys in local storage:
 * - storage_version
 * - token_timeline
 */



/**************************************************
 * REGISTER APP CALLBACKS
 **************************************************/

// Opened app and PebbleKit JS ready
Pebble.addEventListener("ready", function(e) {
	console.log("PebbleKit JS is ready!");

	// Check storage version
	var storage_version_current = parseInt(localStorage.getItem("storage_version"));
	if(isNaN(storage_version_current)) {
		storage_version_current = 0;
	}

	// Upgrade storage if necessary
	if(storage_version_current < storageVersionLatest) {
		switch(storage_version_current) {
			case(0): // Upgrade new users
				localStorage.setItem("storage_version", storageVersionLatest);
				break;
			default: // Upgrade outdated version
				localStorage.clear();
				localStorage.setItem("storage_version", storageVersionLatest);
		}
	}

	// Get timeline token
	if(localStorage.getItem("token_timeline") === null || localStorage.getItem("token_timeline") === "") {
		// No timeline token yet, try to get one
		Pebble.getTimelineToken(
			function(token) { // Success
				console.log("Saving timeline token: " + token);
				localStorage.setItem("token_timeline", token);
				sendReady();
				locationFetch();
			}, function(error) { // Failure
				console.log("Error getting timeline token: " + error);
				sendError(errorTimelineToken);
			}
		);
	} else {
		// Timeline token already present
		sendReady();
		locationFetch();
	}
});

Pebble.addEventListener("showConfiguration", function(e){
	var token_account = Pebble.getAccountToken();
	var token_timeline = localStorage.getItem("token_timeline");
	
	// Get timeline token
	if(token_timeline === null) {
		// No timeline token yet, try to get one
		Pebble.getTimelineToken(
			function(token) { // Success
				console.log("Saving timeline token: " + token);
				localStorage.setItem("token_timeline", token);
				show_config_page(token_account, token);
			}, function(error) { // Failure
				console.log("Error opening configuration screen, can't get timeline token: " + error);
				Pebble.showSimpleNotificationOnPebble("Commute uses timeline", "Grant Commute access to your timeline in the Pebble phone app.");
			}
		);
	} else {
		// Timeline token already present
		show_config_page(token_account, token_timeline);
	}
});

function show_config_page(token_account, token_timeline) {
	console.log("Opened configuration screen on phone");
	Pebble.openURL("https://commute-pebble.appspot.com/config/" + token_account + "?token_timeline=" + encodeURIComponent(token_timeline));
}

Pebble.addEventListener("webviewclosed", function(e){
	console.log("Saved configuration page");
	sendConfigChanged();
});

// Incoming AppMessage -> New request
Pebble.addEventListener("appmessage", function(e) {
	if(e.payload.REQUEST_ID > requestId) {
		console.log("Received new request!");
		requestId = e.payload.REQUEST_ID;
		requestOrig = e.payload.REQUEST_ORIG;
		requestDest = e.payload.REQUEST_DEST;
		requestReceived = true;
		if (locationError) {
			sendError(errorLocation);
		} else if (locationFound) {
			directionsFetch();
		}
	} else {
		console.log("Received outdated request, dropping");
	}
});



/**************************************************
 * LOCATION FUNCTIONS
 **************************************************/

function locationFetch() {
	console.log("Fetching location...");
	navigator.geolocation.getCurrentPosition(locationSuccess, locationFailure, locationOptions);
}

function locationSuccess(pos) {
	console.log("Location found!");
	sendLocated();

	locationLat = pos.coords.latitude;
	locationLon = pos.coords.longitude;
	locationFound = true;
	if (requestReceived) {
		directionsFetch();
	}
}

function locationFailure(err) {
	console.log("Location error (" + err.code + "): " + err.message);
	locationError = true;
	sendError(errorLocation);
}



/**************************************************
 * INTERNET FUNCTIONS
 **************************************************/

var xhrRequest = function (url, type, callback) {
	var xhr = new XMLHttpRequest();
	xhr.onload = function () {
		callback(this.responseText, xhr.status);
	};
	xhr.timeout = 10000;
	xhr.ontimeout = function () {
		console.log("AJAX request timed out");
		sendError(errorInternetTimeout);
	};
	xhr.onerror = function() {
		callback(xhr.status);
	};
	xhr.open(type, url);
	xhr.send();
};

function directionsFetch() {
	console.log("Fetching directions from " + requestOrig + " to " + requestDest + " (request ID: " + requestId + ")");

	// Construct URL
	var token_account = Pebble.getAccountToken();
	var token_timeline = localStorage.getItem("token_timeline");
	var url = gaeBaseUrl + "/directions/" + token_account +
		"?token_timeline=" + token_timeline +
		"&request_orig=" + requestOrig +
		"&request_dest=" + requestDest;
	if (requestOrig === 0 || requestDest === 0) {
		url += "&request_coord=" + locationLat + "," + locationLon;
	}

	// Perform request, handle response
	xhrRequest(url, "GET", function(responseText, statusCode) {
		if(statusCode === 200) {
			var responseJson = JSON.parse(responseText);
			if (responseJson.status === "OK") {
				var via = "via " + responseJson.routes[0].summary;
				if (via.length > 15) { // Truncate via label
					via = via.substring(0,12) + "...";
				} else if (via.length === 4) { // Empty via label
					via = "";
				}
				var response = {
					"requestId": requestId,
					"durationNormal": Math.round(responseJson.routes[0].legs[0].duration.value / 60),
					"durationTraffic": Math.round(responseJson.routes[0].legs[0].duration_in_traffic.value / 60),
					"via": via
				};
				if (response.durationTraffic !== null) {
					directionsSuccess(response);
				} else {
					console.log("Error: no traffic data available");
					sendError(errorResponseNoTrafficData);
				}
			} else if (responseJson.status === "NOT_FOUND") {
				console.log("Error: address incorrect");
				sendError(errorResponseAddressIncorrect);
			} else if(responseJson.status === "ZERO_RESULTS") {
				console.log("Error: no route between addresses");
				sendError(errorResponseNoRoute);
			} else {
				console.log("Error: received unexpected response from GAE (" + responseJson.status + ")");
				sendError(errorResponseUnexpected);
			}
		} else if(statusCode === 404) {
			console.log("Error: app not configured yet");
			sendError(errorConfigure);
		} else if(statusCode === 409) {
			console.log("Error: app reconfiguration required");
			sendError(errorReconfigure);
		} else {
			console.log("Error: unexpected HTTP status as result (" + statusCode + ")");
			sendError(errorInternetUnavailable);
		}
	}, function(statusCode) {
		// Request error
		console.log("Error: unexpected error during request (" + statusCode + ")");
		sendError(errorInternetUnavailable);
	});
}

function directionsSuccess(response) {
	console.log("Directions received!");
	sendDirections(response.requestId, response.durationNormal, response.durationTraffic, response.via);
}



/**************************************************
 * APPMESSAGE FUNCTIONS
 **************************************************/

function sendReady() {
	var dictionary = {
		"RESPONSE_TYPE": responseTypeReady
	};
	sendAppMessage(dictionary, "ready");
}

function sendLocated() {
	var dictionary = {
		"RESPONSE_TYPE": responseTypeLocated
	};
	sendAppMessage(dictionary, "located");
}

function sendDirections(requestId, durationNormal, durationTraffic, via) {
	var dictionary = {
		"RESPONSE_TYPE": responseTypeDirections,
		"RESPONSE_DURATION_NORMAL": durationNormal,
		"RESPONSE_DURATION_TRAFFIC": durationTraffic,
		"RESPONSE_VIA": via,
		"REQUEST_ID": requestId
	};
	sendAppMessage(dictionary, "directions");
}

function sendError(error) {
	var dictionary = {
		"RESPONSE_TYPE": responseTypeError,
		"RESPONSE_ERROR": error
	};
	sendAppMessage(dictionary, "error");
}

function sendConfigChanged() {
	var dictionary = {
		"RESPONSE_TYPE": responseTypeConfigChanged
	};
	sendAppMessage(dictionary, "config changed");
}

function sendAppMessage(dictionary, description) {
	Pebble.sendAppMessage(dictionary, function(e){
		// Pebble ACK
		console.log("Pebble ACK (" + description + "), transactionId=" + e.data.transactionId);
	}, function(e){
		// Pebble NACK
		console.log("Pebble NACK (" + description + "), transactionId=" + e.data.transactionId + ", error: " + e.error.message);
	});
}
