#ifndef PENTAX_HH
#define PENTAX_HH

#include <string>
#include <list>
#include <map>

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

class Stop
{
public:
    Stop(int stops = 1);
    static Stop fromHalfStops(int);
    static Stop fromThirdStops(int);

    static Stop fromAperture(float);
    static Stop fromShutterspeed(float);
    static Stop fromShuttertime(float);
    static Stop fromISO(int);
    static Stop fromExposureCompensation(float);
    static Stop fromExposureSteps(int);

    Stop & operator+=(const Stop &);
    Stop & operator-=(const Stop &);
    bool operator!=(const Stop &) const;
    bool operator==(const Stop &) const;

    const Stop operator+(const Stop &) const;
    const Stop operator-(const Stop &) const;
    const Stop operator*(int) const;

    const Stop plus(const Stop &) const;
    const Stop minus(const Stop &) const;
    const Stop times(int) const;

    float asAperture() const;
    float asShutterspeed() const;
    float asShuttertime() const;
    int asISO() const;
    float asExposureCompensation() const;
    int asInt() const;
    int asExposureSteps() const;

    bool isAuto() const;
    bool isUnknown() const;

    std::string getPrettyString() const;
    std::string getSecondsString() const;
    std::string getOneOverString(bool denomOnly = false) const;
    std::string getApertureString() const;
    std::string getISOString() const;

public:
    static const Stop UNKNOWN;
    static const Stop AUTO;
    static const Stop HALF;
    static const Stop THIRD;
protected:
    static Stop fromSixthStops(int);
    int sixthStops;
};

/** Singleton class representing the camera connected.
 * Get the only instance of this class calling the camera() method.
 */
class Camera
{
public:
    typedef std::string Parameter;
    typedef std::string String;
public:
    void setStop(const Parameter &, const Stop &);
    void setString(const Parameter &, const String &);
    void setStopByIndex(const Parameter &, int);
    void setStringByIndex(const Parameter &, int);
    
    Stop getStop(const Parameter &) const;
    String getString(const Parameter &) const;
    Stop getMinimum(const Parameter &) const;
    Stop getMaximum(const Parameter &) const;
    int getStopCount(const Parameter &) const;
    int getStringCount(const Parameter &) const;
    Stop getStopOption(const Parameter &, int) const;
    String getStopOptionAsString(const Parameter &, int) const;
    String getStringOption(const Parameter &, int) const;

    void startUpdating(long ms = 2500);
    void stopUpdating();
    void applyChanges();
    void updateValues();

    bool setFileDestination(std::string);
    std::string getFileDestination() const;
    std::string getStatusInformation() const;
    std::string getFilename();

    void focus();
    std::string shoot();
protected:
    std::map<Parameter, Stop> requestedStopChanges;
    std::map<Parameter, String> requestedStringChanges;
    std::map<Parameter, Stop> currentStopValues;
    std::map<Parameter, String> currentStringValues;

    std::string path;
    std::string lastFilename;
    int imageNumber;

public:
    static const std::string API_VERSION;

public:
    ~Camera();
    
protected:
    Camera();
    friend const Camera * camera();
    bool saveBuffer(const std::string & filename);
    void deleteBuffer();
    std::string getFileExtension();
};

/** Returns the single Camera instance.
 * If the camera is connected, and the user has write permissions for it, returns a Camera object,
 * else it returns NULL, indicating failure.
 */
const Camera * camera();
#endif
