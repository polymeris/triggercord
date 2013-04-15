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

    Stop & operator+=(const Stop &);
    Stop & operator-=(const Stop &);

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

    std::string getPrettyString() const;
    std::string getSecondsString() const;
    std::string getOneOverString() const;

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
    void set(const Parameter &, const Stop &);
    void set(const Parameter &, const String &);
    void set(const Parameter &, int);
    Stop get(const Parameter &) const;
    String getString(const Parameter &) const;
    int getMinimum(const Parameter &) const;
    int getMaximum(const Parameter &) const;
    String getStringOption(const Parameter &, int);

    void startUpdating(long ms = 2500);
    void stopUpdating();
    void applyChanges();
    void updateValues();
protected:
    std::map<Parameter, Stop> requestedStopChanges;
    std::map<Parameter, String> requestedStringChanges;
    std::map<Parameter, Stop> currentStopValues;
    std::map<Parameter, String> currentStringValues;

public:
    ~Camera();
    
protected:
    Camera();
    friend const Camera * camera();
public:
    static const std::string STRING_VALUE_PARAMETERS[];
    static const std::string STOP_VALUE_PARAMETERS[];
};

/** Returns the single Camera instance.
 * If the camera is connected, and the user has write permissions for it, returns a Camera object,
 * else it returns NULL, indicating failure.
 */
const Camera * camera();
#endif
