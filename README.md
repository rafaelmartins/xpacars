# xpacars

WIP

X-Plane 11 plugin to feed data to myACARS (a smartCARS-alike plugin for Linux).

This plugin only works on Linux!


## How to build and use?

After installing the dependencies (libcurl), run:

    $ wget http://developer.x-plane.com/wp-content/plugins/code-sample-generation/sample_templates/XPSDK301.zip
    $ unzip XPSDK301.zip
    $ ./autogen.sh  # (if running from Git, depends on GNU autotools)
    $ ./configure
    $ make plugin

To install the plugin, run:

    $ export XPLANE_DIR=...   # use the directory where you installed X-Plane 11
    $ unzip xpacars-plugin-0.1.zip -d "${XPLANE_DIR}/Resources/plugins/"

Edit the `${XPLANE_DIR}/Resources/plugins/xpacars.txt` file, to point to the URL
of the endpoint you want to `POST` your position data to. This endpoint should
be served through HTTPS for production environment, and include basic auth support.
The user and password should be provided in the URL.


## Protocol

```c
/**
 * xpacars protocol, version 1
 *
 * This is the definition of our protocol. All the fields are separated with
 * a newline '\n' character.
 *
 * We have 2 types of requests, that are defined by the Content-Type header.
 */

/**
 * Content-Type: application/vnd.xpacars.flight
 *
 * This request registers a new flight in the server. It just contains the
 * aircraft data for now, but information about origin airport and destination
 * aiport should be added in a new version of the protocol.
 *
 * The server should return 201 to notify that the new flight was registered,
 * and the ID of the flight, that will be used by the upcoming requests.
 */
const char *flight_format =
    "1\n"   // protocol version
    "%s\n"  // aircraft icao
    "%s\n"  // aircraft tailnum
    "%s\n"  // aircraft description
    "";

/**
 * Content-Type: application/vnd.xpacars.position
 *
 * This request send information about the current status of the aircraft to
 * the server.
 *
 * The server should return 202 to notify that accepted the data. When a
 * request is not accepted, the client may take some action to recover, like
 * stop sending for a while, or increase the send intervals.
 *
 * There's no guarantee that the data was processed by the server right after
 * the request. The server can do the processing asyncronously.
 */
const char *position_format =
    "1\n"     // protocol version
    "%lld\n"  // flight id
    "%f\n"    // latitude in degrees
    "%f\n"    // longitude in degrees
    "%f\n"    // altitude in meters (not feets!)
    "%f\n"    // track in degrees
    "%f\n"    // ground speed in meters per second (not knots!)
    "%f\n"    // air speed in meters per second (not knots!)
    "%f\n"    // vertical speed meters per second (not feets per minute!)
    "";
```
