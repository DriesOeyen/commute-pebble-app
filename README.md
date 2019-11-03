# Commute for Pebble (Pebble watch app)
Commute is an application for the Pebble smartwatch that allows users to look up commute times from their current location to home and work.
Integration with Pebble Timeline allows users to receive proactive notifications when it's time to leave for work.
The app is backed by data from the Google Maps API.

This is the Pebble watch app. All relevant repos:

- :watch: Pebble watch app (required): this repository
- :cloud: Server back-end (required): https://github.com/DriesOeyen/commute-pebble-gae/
- :bulb: Activity indicator LED-strip (optional): https://github.com/DriesOeyen/commute-pebble-headlights/

## Build instructions
**Before you start:** the default server back-end for this app is no longer available.
In order to get Commute up-and-running, deploy your own server back-end (repo linked above) and change the value of `gaeBaseUrl` in `src/pkjs/index.js` before compiling the watch app.

Compile the app with Pebble SDK version 3. Device compatibility:

- Pebble Classic
- Pebble Steel
- Pebble Time
- Pebble Time Steel
- Pebble Time Round
- Pebble 2
- Pebble Time 2 (unfinished, see `emery` branch)
