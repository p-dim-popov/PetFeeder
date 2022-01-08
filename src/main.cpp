#include <Arduino.h>

#include <../.pio/libdeps/sparkfun_promicro16/Servo/src/Servo.h> // that include is bad...

// TODO: Separate functionalities into private libraries

#define bwGet(state, x) (bool)(state & x)
#define bwSet(state, x, value) (value ? state |= x : state &= ~x)
#define getMillisDiff(ms, prevMs) ((unsigned long)(ms - prevMs))

struct {
    static void println(const char level, const char* arg, bool appendNewLine) {
        Serial.print(level);
        Serial.print("/");
        if (appendNewLine) Serial.println(arg);
        else Serial.print(arg);
    }

    /**
     * Levels:
     *  - no logs 0
     *  - error 1
     *  - warn 2
     *  - info 3
     *  - debug
     */
    uint8_t level = 3;
    bool debugOn = true;

    void info(const char* arg = "", bool appendNewLine = true) const {
        if (level >= 3) println('i', arg, appendNewLine);
    }
    void warn(const char* arg = "", bool appendNewLine = true) const {
        if (level >= 2) println('w', arg, appendNewLine);
    }
    void error(const char* arg = "", bool appendNewLine = true) const {
        if (level >= 1) println('e', arg, appendNewLine);
    }
    void debug(const char* arg = "", bool appendNewLine = true) const {
        if (debugOn) println('d', arg, appendNewLine);
    }
} logger;

struct Time {
    uint8_t milliseconds = 0;
    uint8_t seconds = 0;
    uint8_t minutes = 0;
    uint8_t hours = 0;

    uint32_t toMs() const {
        return (hours * 3600000) + (minutes * 60000) + (seconds * 1000) + milliseconds;
    }

    static Time now();
    static Time fromMs(unsigned long long duration);
    static void set(const Time& time);

private:
    static uint32_t prevMillis;
    static uint32_t timeElapsed;
};

Time started;

uint32_t Time::prevMillis = millis();
uint32_t Time::timeElapsed = 0;

Time Time::now() {
    auto currentMillis = millis();
    timeElapsed += getMillisDiff(currentMillis, prevMillis);
    prevMillis = currentMillis;
    return Time::fromMs(started.toMs() + timeElapsed);
}

Time Time::fromMs(unsigned long long duration) {
    Time time = {};
    time.milliseconds = (duration % 1000) / 100;
    time.seconds = (duration / 1000) % 60;
    time.minutes = (duration / 60000) % 60;
    time.hours = (duration / 3600000) % 24;
    return time;
}

void Time::set(const Time &time) {
    started = time;
}

class IReact { virtual void react() = 0; };

struct Led {
    int _pin;
    explicit Led(int pin): _pin(pin) { pinMode(pin, OUTPUT); }

    void turnOn() const { digitalWrite(_pin, HIGH); }
    void turnOff() const { digitalWrite(_pin, LOW); }
    void toggle() const { digitalWrite(_pin, !digitalRead(_pin)); }
};

template<typename TContext> struct Button : IReact {
    explicit Button(
            int pin,
            TContext& context,
            void(*onClick)(TContext&) = nullptr,
            void(*onHold)(TContext&) = nullptr,
            void(*onRelease)(TContext&) = nullptr
        ):
            _pin(pin),
            _context(context),
            _onClick(onClick),
            _onHold(onHold),
            _onRelease(onRelease)
    {
        pinMode(pin, INPUT);
    }

    void react() override {
        auto prevIsHigh = bwGet(_state, IS_HIGH);
        auto isHigh = bwGet(bwSet(_state, IS_HIGH, digitalRead(_pin)), IS_HIGH);

        if (!prevIsHigh && isHigh) onDown();
        else if (prevIsHigh && isHigh && !bwGet(_state, IS_BEING_HELD) && (getMillisDiff(millis(), _downStartTime) > BUTTON_HOLD_DIFF_MS)) onStartHolding();
        else if (prevIsHigh && !isHigh) onUp();
    }

private:
    static const uint16_t BUTTON_HOLD_DIFF_MS = 500;
    static const uint8_t IS_HIGH = 0b00000001;
    static const uint8_t IS_BEING_HELD = 0b00000010;

    int _pin = -1;
    uint32_t _downStartTime = 0;
    uint8_t _state = 0b00000000;
    TContext& _context;

#pragma region handlers
    void(*_onClick)(TContext&);
    void onClick() {
        if (_onClick) _onClick(_context);
    }

    void(*_onHold)(TContext&);
    void onHold() {
        if (_onHold) _onHold(_context);
    }

    void(*_onRelease)(TContext&);
    void onRelease() {
        if (_onRelease) _onRelease(_context);
    }
#pragma endregion handlers

    void onStartHolding() {
        bwSet(_state, IS_BEING_HELD, true);
        onHold();
    }

    void onDown() {
        _downStartTime = millis();
        logger.info("button down");
    }

    void onUp() {
        logger.info("button up");
        bwSet(_state, IS_HIGH, false);
        bwSet(_state, IS_BEING_HELD, false);

        auto releaseTime = millis();
        if (getMillisDiff(releaseTime, _downStartTime) <= BUTTON_HOLD_DIFF_MS) onClick();
        else onRelease();
    }
};

struct ServoRotator: IReact {
    explicit ServoRotator(const int pin): _pin{pin} {
        _servo.attach(pin);
        _servo.write(CLOSED_DEGREES);
    }

    void open() {
        _servo.write(OPENED_DEGREES);
        bwSet(state, IS_OPEN, true);
    }

    template<typename TTimePeriodMs = uint16_t> void openTimed(TTimePeriodMs timePeriodMs = DEFAULT_OPEN_TIME_MS) {
        open();
        bwSet(state, IS_TIMED, true);
        _openedTime = millis();
        _closeTimePeriodMs =  timePeriodMs;
    }

    void close() {
        logger.info("closing");
        bwSet(state, IS_OPEN, false);
        bwSet(state, IS_TIMED, false);
        _servo.write(CLOSED_DEGREES);
    }

    void react() override {
        if (bwGet(state, IS_OPEN) && bwGet(state, IS_TIMED) && getMillisDiff(millis(), _openedTime) >= _closeTimePeriodMs) close();
    }

private:
    static const uint16_t DEFAULT_OPEN_TIME_MS = 2000;
    static const uint8_t OPENED_DEGREES = 180;
    static const uint8_t CLOSED_DEGREES = 0;

    static const uint8_t IS_OPEN = 0b00000001;
    static const uint8_t IS_TIMED = 0b00000010;

    Servo _servo;
    uint8_t state = 0b00000000;
    int _pin = 0;
    uint32_t _openedTime = 0;
    uint32_t _closeTimePeriodMs = 0;
};

struct Program {
    Led redLed{13};
    ServoRotator servoRotator{9};
    Button<Program> rotatorButton{
            2,
            *this,
            [](Program &program) {
                logger.debug("clicked");
                program.redLed.toggle();
                program.servoRotator.openTimed();
                Time::set(Time::fromMs(1641669327069)); // 19:15:28
            },
            [](Program &program) {
                logger.debug("held");
                program.redLed.turnOn();
                program.servoRotator.open();
            },
            [](Program &program) {
                logger.debug("released");
                program.redLed.turnOff();
                program.servoRotator.close();
            }
    };

    Program() {
        Serial.begin(9600);
    }

    void act() {
        rotatorButton.react();
        servoRotator.react();
        auto time = Time::now();
        String res;
        res.concat(time.hours);
        res.concat(":");
        res.concat(time.minutes);
        res.concat(":");
        res.concat(time.seconds);
        logger.debug(res.c_str());
    }
};

Program* program;

__attribute__((unused)) void setup() { program = new Program(); }
__attribute__((unused)) void loop() { program->act(); }
