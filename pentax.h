#ifndef PENTAX_HH
#define PENTAX_HH

#include <string>
#include <list>

#ifdef SWIG
    %module pentax
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
    const std::string & model();
    
    void focus();
    std::string shoot();
    
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

    void setAperture(float aperture);
    void setShutter(float shutter);
    void setIso(int iso, int min = KEEP, int max = KEEP);
    void setExposureCompensation(float ec);
    void setAutofocusPoint(int point);

    void setRaw(bool enabled);
    void setJpegAdjustments(
        int saturation, int hue,
        int contrast, int sharpness);

    bool setFileDestination(std::string path);
    std::string fileDestination();
    std::string fileExtension();

    float aperture();
    float maximumAperture();
    float minimumAperture();
    float shutter();
    float maximumShutter();
    float minimumShutter();
    float iso();
    float maximumIso();
    float minimumIso();
    float maximumAutoIso();
    float minimumAutoIso();
    float exposureCompensation();
    float maximumExposureCompensation();
    float minimumExposureCompensation();
    int focusPoint();
    int meteringMode();
    int autofocusMode();
    bool raw();
    
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

const Camera * camera();
#endif
