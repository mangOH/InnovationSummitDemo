//--------------------------------------------------------------------------------------------------
/** @file
 * This app reads the current gps location every 60 seconds and sends it to the data router.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 * The time between publishing GPS location readings
 *
 * @note Please change this timeout value as needed.
 */
//--------------------------------------------------------------------------------------------------
#define GPS_SAMPLE_INTERVAL_IN_SECONDS (60)

//--------------------------------------------------------------------------------------------------
/**
 * How long to retry sampling the GPS location before publishing a hard-coded location.
 *
 * @note This value should be < GPS_SAMPLE_INTERVAL_IN_SECONDS
 */
//--------------------------------------------------------------------------------------------------
#define GPS_RETRY_PERIOD_IN_SECONDS (59)

#define KEY_GPS_LATITUDE      "sensors.mangoh.gps.latitude"
#define KEY_GPS_LONGITUDE     "sensors.mangoh.gps.longitude"

static const int32_t PINNACLE_HOTEL_LONGITUDE = -123120900;
static const int32_t PINNACLE_HOTEL_LATITUDE = 4928790;

//--------------------------------------------------------------------------------------------------
/**
 * Attempts to use the GPS to find the current latitude, longitude and horizontal accuracy within
 * the given timeout constraints.
 *
 * @return
 *      - LE_OK on success
 *      - LE_UNAVAILABLE if positioning services are unavailable
 *      - LE_TIMEOUT if the timeout expires before successfully acquiring the location
 *
 * @note
 *      Blocks until the location has been identified or the timeout has occurred.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GetCurrentLocation
(
    int32_t *latitudePtr,           ///< [OUT] latitude of device - set to NULL if not needed
    int32_t *longitudePtr,          ///< [OUT] longitude of device - set to NULL if not needed
    int32_t *horizontalAccuracyPtr, ///< [OUT] horizontal accuracy of device - set to NULL if not
                                    ///< needed
    uint32_t timeoutInSeconds       ///< [IN] duration to attempt to acquire location data before
                                    ///< giving up.  A value of 0 means there is no timeout.
)
{
    le_posCtrl_ActivationRef_t posCtrlRef = le_posCtrl_Request();
    if (!posCtrlRef)
    {
        LE_ERROR("Can't activate the Positioning service");
        return LE_UNAVAILABLE;
    }

    le_result_t result;
    const time_t startTime = time(NULL);
    LE_INFO("Checking GPS position");
    while (true)
    {
        result = le_pos_Get2DLocation(latitudePtr, longitudePtr, horizontalAccuracyPtr);
        if (result == LE_OK)
        {
            break;
        }
        else if (
            (timeoutInSeconds != 0) &&
            (difftime(time(NULL), startTime) > (double)timeoutInSeconds))
        {
            result = LE_TIMEOUT;
            break;
        }
        else
        {
            // Sleep for one second before requesting the location again.
            sleep(1);
        }
    }

    le_posCtrl_Release(posCtrlRef);

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Timer handler that will publish the current GPS location to the data router.
 *
 * @note
 *      If the location cannot be determined, a hard-coded location will be published.
 */
//--------------------------------------------------------------------------------------------------
static void gpsTimer
(
    le_timer_Ref_t gpsTimerRef
)
{
    int32_t latitude;
    int32_t longitude;
    int32_t horizontalAccuracy;

    const le_result_t result = GetCurrentLocation(
        &latitude, &longitude, &horizontalAccuracy, GPS_RETRY_PERIOD_IN_SECONDS);
    if (result == LE_OK)
    {
        LE_INFO("Location from GPS is: %d, %d", latitude, longitude);
    }
    else
    {
        latitude = PINNACLE_HOTEL_LATITUDE;
        longitude = PINNACLE_HOTEL_LONGITUDE;
        LE_INFO(
            "Couldn't get GPS location.  Publishing Pinnacle Hotel location: %d, %d",
            latitude,
            longitude);
    }
    const int32_t now = time(NULL);
    dataRouter_WriteInteger(KEY_GPS_LATITUDE, latitude, now);
    dataRouter_WriteInteger(KEY_GPS_LONGITUDE, longitude, now);
}

COMPONENT_INIT
{
    dataRouter_SessionStart("", "", false, DATAROUTER_CACHE);

    le_clk_Time_t  timerInterval = {.sec = GPS_SAMPLE_INTERVAL_IN_SECONDS, .usec = 0};
    le_timer_Ref_t gpsTimerRef;
    gpsTimerRef = le_timer_Create("GPS Timer");
    le_timer_SetInterval(gpsTimerRef, timerInterval);
    le_timer_SetRepeat(gpsTimerRef, 0);
    le_timer_SetHandler(gpsTimerRef, gpsTimer);
    // Explicitly call the timer handler so that the app publishes a GPS location immediately
    // instead of waiting for the first timer expiry to occur.
    gpsTimer(gpsTimerRef);
    le_timer_Start(gpsTimerRef);
}