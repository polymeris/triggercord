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

#ifdef SWIGJAVA
    #define AUTO       (-1)
    #define SINGLE     (-2)
    #define CONTINUOUS (-3)
    #define CENTER     (-4)
    #define SPOT       (-5)
    #define KEEP     (-100)
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
    Stop stop(const Parameter &) const;
    String string(const Parameter &) const;
    int minimum(const Parameter &) const;
    int maximum(const Parameter &) const;
    String accepptedString(const Parameter &, int);

protected:
    std::map<Parameter, Stop> requestedStopChanges;
    std::map<Parameter, String> requestedStringChanges;
    std::map<Parameter, Stop> currentStopValues;
    std::map<Parameter, String> currentStringValues;
    void applyChanges();
    void updateValues();
    
public:
    ~Camera();
    
protected:
    Camera();
    friend const Camera * camera();
public:
    static const std::string STRING_VALUE_PARAMETERS[];
public:
    static const int UNKNOWN = -1000;
};

/** Returns the single Camera instance.
 * If the camera is connected, and the user has write permissions for it, returns a Camera object,
 * else it returns NULL, indicating failure.
 */
const Camera * camera();
#endif
