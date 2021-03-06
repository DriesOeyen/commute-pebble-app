/* global Pebble:true, console:true */

var keys = require("message_keys");
var token_timeline = "";

var locationLat;
var locationLon;
var locationFound = false;
var locationError = false;
var locationOptions = {
	enableHighAccuracy: true,
	maximumAge: 120000,
	timeout: 10000
};

var requestIdLatest = -1;
var requestOrig;
var requestDest;
var preferenceAmPm;
var requestReceived = false;

var responseTypeReady = 0;
var responseTypeLocated = 1;
var responseTypeDirections = 2;
var responseTypeError = 3;
var responseTypeConfigChanged = 4;

//var errorTimelineToken = 0;
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
 * APPMESSAGE FUNCTIONS
 **************************************************/

function sendAppMessage(dictionary, description) {
	Pebble.sendAppMessage(dictionary, function(e){
		// Pebble ACK
		console.log("Pebble ACK (" + description + "), transactionId=" + e.data.transactionId);
	}, function(e){
		// Pebble NACK
		console.log("Pebble NACK (" + description + "), transactionId=" + e.data.transactionId);
	});
}

function sendReady() {
	var dictionary = {};
	dictionary[keys.RESPONSE_TYPE] = responseTypeReady;
	sendAppMessage(dictionary, "ready");
}

function sendLocated() {
	var dictionary = {};
	dictionary[keys.RESPONSE_TYPE] = responseTypeLocated;
	sendAppMessage(dictionary, "located");
}

function sendDirections(requestId, durationNormal, durationTraffic, via) {
	var dictionary = {};
	dictionary[keys.RESPONSE_TYPE] = responseTypeDirections;
	dictionary[keys.RESPONSE_DURATION_NORMAL] = durationNormal;
	dictionary[keys.RESPONSE_DURATION_TRAFFIC] = durationTraffic;
	dictionary[keys.RESPONSE_VIA] = via;
	dictionary[keys.REQUEST_ID] = requestId;
	sendAppMessage(dictionary, "directions");
}

function sendError(error) {
	var dictionary = {};
	dictionary[keys.RESPONSE_TYPE] = responseTypeError;
	dictionary[keys.RESPONSE_ERROR] = error;
	sendAppMessage(dictionary, "error");
}

function sendConfigChanged() {
	var dictionary = {};
	dictionary[keys.RESPONSE_TYPE] = responseTypeConfigChanged;
	sendAppMessage(dictionary, "config changed");
}



/**************************************************
 * INTERNET FUNCTIONS
 **************************************************/

var xhrRequest = function(url, type, loadCallback, errorCallback) {
	var xhr = new XMLHttpRequest();
	xhr.onload = function() {
		loadCallback(xhr.status, this.responseText);
	};
	xhr.timeout = 10000;
	xhr.ontimeout = function() {
		console.log("AJAX request timed out");
		sendError(errorInternetTimeout);
	};
	xhr.onerror = function() {
		errorCallback();
	};
	xhr.open(type, url);
	xhr.send();
};

function directionsFetch(requestId) {
	console.log("Fetching directions from " + requestOrig + " to " + requestDest + " (request ID: " + requestId + ")");

	// Construct URL
	var token_account = Pebble.getAccountToken();
	var url = gaeBaseUrl +
		"/directions/" + encodeURIComponent(token_account) +
		"?token_timeline=" + encodeURIComponent(token_timeline) +
		"&request_orig=" + encodeURIComponent(requestOrig) +
		"&request_dest=" + encodeURIComponent(requestDest) +
		"&am_pm=" + encodeURIComponent(preferenceAmPm.toString());
	if (requestOrig === 0 || requestDest === 0) {
		url += "&request_coord=" + encodeURIComponent(locationLat) + "," + encodeURIComponent(locationLon);
	}

	// Perform request, handle response
	xhrRequest(url, "GET", function(statusCode, responseText) {
		if (statusCode === 200) {
			var responseJson = JSON.parse(responseText);
			if (responseJson.status === "OK") {
				if (responseJson.routes[0].legs[0].duration_in_traffic !== undefined) {
					// Directions with traffic data received
					console.log("Directions received!");
					var durationNormal = Math.round(responseJson.routes[0].legs[0].duration.value / 60);
					var durationTraffic = Math.round(responseJson.routes[0].legs[0].duration_in_traffic.value / 60);
					var via = "via " + responseJson.routes[0].summary;
					if (via.length > 15) { // Truncate via label
						via = via.substring(0,12) + "...";
					} else if (via.length === 4) { // Empty via label
						via = "";
					}
					sendDirections(requestId, durationNormal, durationTraffic, via);
				} else {
					// No traffic data
					console.log("Error: no traffic data available");
					sendError(errorResponseNoTrafficData);
				}
			} else if (responseJson.status === "NOT_FOUND") {
				console.log("Error: address incorrect");
				sendError(errorResponseAddressIncorrect);
			} else if (responseJson.status === "ZERO_RESULTS") {
				console.log("Error: no route between addresses");
				sendError(errorResponseNoRoute);
			} else {
				console.log("Error: received unexpected response from GAE (" + responseJson.status + ")");
				sendError(errorResponseUnexpected);
			}
		} else if (statusCode === 404) {
			console.log("Error: app not configured yet");
			sendError(errorConfigure);
		} else if (statusCode === 409) {
			console.log("Error: app reconfiguration required");
			sendError(errorReconfigure);
		} else {
			console.log("Error: unexpected HTTP status as result (" + statusCode + ")");
			sendError(errorResponseUnexpected);
		}
	}, function() {
		// Request error
		console.log("Error: network error during request");
		sendError(errorInternetUnavailable);
	});
}



/**************************************************
 * LOCATION FUNCTIONS
 **************************************************/

function locationSuccess(pos) {
	console.log("Location found!");
	sendLocated();

	locationLat = pos.coords.latitude;
	locationLon = pos.coords.longitude;
	locationFound = true;
	if (requestReceived) {
		directionsFetch(requestIdLatest);
	}
}

function locationFailure(err) {
	console.log("Location error (" + err.code + "): " + err.message);
	locationError = true;
	sendError(errorLocation);
}

function locationFetch() {
	console.log("Fetching location...");
	navigator.geolocation.getCurrentPosition(locationSuccess, locationFailure, locationOptions);
}



/**************************************************
 * REGISTER APP CALLBACKS
 **************************************************/

// Opened app and PebbleKit JS ready
Pebble.addEventListener("ready", function() {
	console.log("PebbleKit JS is ready!");

	// Check storage version
	var storage_version_current = parseInt(localStorage.getItem("storage_version"));
	if (isNaN(storage_version_current)) {
		storage_version_current = 0;
	}

	// Upgrade storage if necessary
	if (storage_version_current < storageVersionLatest) {
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
	Pebble.getTimelineToken(
		function(token) { // Success
			token_timeline = token;
			localStorage.setItem("token_timeline", token_timeline);
			console.log("Saving timeline token: " + token_timeline);
			sendReady();
			locationFetch();
		}, function(error) { // Failure
			token_timeline = localStorage.getItem("token_timeline");
			if (token_timeline === null) {
				token_timeline = "";
			}
			console.log("Error getting timeline token (" + error + "), using old token: " + token_timeline);
			sendReady();
			locationFetch();
		}
	);
});

// Open configuration page
Pebble.addEventListener("showConfiguration", function() {
	console.log("Opened configuration screen on phone");
	var token_account = Pebble.getAccountToken();
	Pebble.openURL("https://commute-pebble.appspot.com/config/" + encodeURIComponent(token_account));
});

Pebble.addEventListener("webviewclosed", function() {
	console.log("Saved configuration page");
	sendConfigChanged();
});

// Incoming AppMessage -> New request
Pebble.addEventListener("appmessage", function(e) {
	if (e.payload.REQUEST_ID > requestIdLatest) {
		console.log("Received new request!");
		requestIdLatest = e.payload.REQUEST_ID;
		requestOrig = e.payload.REQUEST_ORIG;
		requestDest = e.payload.REQUEST_DEST;
		preferenceAmPm = e.payload.PREFERENCE_AM_PM === 1;
		requestReceived = true;
		if (locationError) {
			sendError(errorLocation);
		} else if (locationFound) {
			directionsFetch(e.payload.REQUEST_ID);
		}
	} else {
		console.log("Received outdated request, dropping");
	}
});
