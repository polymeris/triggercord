#ifndef PENTAX_HH
#define PENTAX_HH

#include <string>
#include <list>

#ifdef SWIG
    %module pentax
    %include "std_string.i"
    #ifdef SWIGJAVA
        %javaconst(1);
    #endif
    %header %{
        #include "pentax.h"
    %}
#endif

#ifdef SWIGJAVA
    #define AUTO       (-1)
    #define SINGLE     (-2)
    #define CONTINUOUS (-3)
    #define CENTER     (-4)
    #define SPOT       (-5)
    #define KEEP     (-100)
#endif

/** Singleton class representing the camera connected.
 * Get the only instance of this class calling the camera() method.
 */
class Camera
{
#ifndef SWIGJAVA
public:
    static const int AUTO       = -1;
    static const int SINGLE     = -2;
    static const int CONTINUOUS = -3;
    static const int CENTER     = -4;
    static const int SPOT       = -5;
    static const int KEEP     = -100;
#endif
public:
    /** Returns the name of the camera model. */
    const std::string model();
    
    void focus();
    /** Shoots a single frame.
     * Returns the filename this shot was saved to.
     */
    std::string shoot();

    /** Sets two or three exposure parameters at the same time. */
    void setExposure(
        float aperture,
        float shutter,
        int iso = KEEP);

    void setMetering(
        int mode,
        float compensation = 0.0);

    void setAutofocus(
        int mode,
        int point = CENTER);

    /** Set the aperture.
     * In N numbers.
     */
    void setAperture(float aperture);
    /** Set the shutterspeed (exposure time).
     * In seconds.
     */
    void setShutter(float shutter);
    /** Sets ISO sensitivity, and an optional Auto ISO range. */
    void setIso(int iso, int min = KEEP, int max = KEEP);
    /** Sets exposure compensation value, in stops. */
    void setExposureCompensation(float ec);
    void setAutofocusPoint(int point);

    /** Enable or disable RAW
     * Currently only DNG is supported, and if RAW is enabled JPEG files will not be saved.
     * That is, RAW+ is not available.
     */
    void setRaw(bool enabled);
    void setJpegAdjustments(
        int saturation, int hue,
        int contrast, int sharpness);

    /** Set destination path for image files.
     * The directory must already exist. Returns false on failure.
     */
    bool setFileDestination(std::string path);
    std::string fileDestination();
    /** Returns the file extension used.
     * Whitout the dot.
     */
    std::string fileExtension();

    /** Returns current aperture in N. */
    float aperture();
    /** Returns maximum aperture number (smallest physical aperture) of the mounted lens. */
    float maximumAperture();
    /** Returns minimum aperture number (largest physical aperture) of the mounted lens. */
    float minimumAperture();
    /** Returns current shutterspeed in seconds. */
    float shutter();
    /** Returns slowest shutterspeed (maximum time) in seconds.
     * Does not consider bulb mode. */
    float maximumShutter();
    /** Returns fastest shutterspeed (minimum time) in seconds. */
    float minimumShutter();
    /** Returns current ISO sensitivity. */
    float iso();
    /** Returns upper sensitivity bound of automatic ISO range. */
    float maximumAutoIso();
    /** Returns lower sensitivity bound of automatic ISO range. */
    float minimumAutoIso();
    /** Returns highest sensitivity the connected camera supports. */
    float maximumIso();
    /** Returns lowest sensitivity the connected camera supports. */
    float minimumIso();
    /** Returns current exposure compensation (EC) setting in stops. */
    float exposureCompensation();
    float maximumExposureCompensation();
    float minimumExposureCompensation();
    int focusPoint();
    int exposureMode();
    int autofocusMode();
    /** Returns true if RAW mode has been selected.
     * This mode saves the RAW data to a DNG file.
     */
    bool raw();
    std::string exposureModeAbbreviation();

public:
    ~Camera();
protected:
    Camera();
    friend const Camera * camera();

    void updateStatus();
    void updateExposureMode();

    bool saveBuffer(const std::string &);
    void deleteBuffer();
    std::string getFilename();

    std::string path;
    std::string lastFilename;
    int imageNumber;
};

/** Returns the single Camera instance.
 * If the camera is connected, and the user has write permissions for it, returns a Camera object,
 * else it returns NULL, indicating failure.
 */
const Camera * camera();
#endif
