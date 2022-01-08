#include <Arduino.h>

#include <../.pio/libdeps/sparkfun_promicro16/Servo/src/Servo.h> // that include is bad...

// TODO: Separate functionalities into private libraries

#define bwGet(state, x) (bool)(state & x)
#define bwSet(state, x, value) (value ? state |= x : state &= ~x)
#define getMillisDiff(ms, prevMs) ((unsigned long)(ms - prevMs))

struct {
    static void println(const char level, const char* arg) {
        Serial.print(level);
        Serial.print("/");
        Serial.println(arg);
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
    bool debugOn = false;

    void info(const char* arg = "") const {
        if (level >= 3) println('i', arg);
    }
    void warn(const char* arg = "") const {
        if (level >= 2) println('w', arg);
    }
    void error(const char* arg = "") const {
        if (level >= 1) println('e', arg);
    }
    void debug(const char* arg = "") const {
        if (debugOn) println('d', arg);
    }
} logger;

struct Led {
    int pin;
    explicit Led(int pin): pin(pin) { pinMode(pin, OUTPUT); }

    void turnOn() const { digitalWrite(pin, HIGH); }
    void turnOff() const { digitalWrite(pin, LOW); }
    void toggle() const { digitalWrite(pin, !digitalRead(pin)); }
};

template<typename TContext> struct Button {
    static const uint16_t BUTTON_HOLD_DIFF_MS = 500;
    static const uint8_t IS_HIGH = 0b00000001;
    static const uint8_t IS_BEING_HELD = 0b00000010;

    int m_pin = -1;
    uint32_t downStartTime = 0;
    uint8_t state = 0b00000000;
    TContext& m_context;

    explicit Button(
            int pin,
            TContext& context,
            void(*onClick)(TContext&) = nullptr,
            void(*onHold)(TContext&) = nullptr,
            void(*onRelease)(TContext&) = nullptr
        ):
        m_pin(pin),
        m_context(context),
        m_onClick(onClick),
        m_onHold(onHold),
        m_onRelease(onRelease)
    {
        pinMode(pin, INPUT);
    }

    void listen() {
        auto prevIsHigh = bwGet(state, IS_HIGH);
        auto isHigh = bwGet(bwSet(state, IS_HIGH, digitalRead(m_pin)), IS_HIGH);

        if (!prevIsHigh && isHigh) onDown();
        else if (prevIsHigh && isHigh && !bwGet(state, IS_BEING_HELD) && (getMillisDiff(millis(), downStartTime) > BUTTON_HOLD_DIFF_MS)) onStartHolding();
        else if (prevIsHigh && !isHigh) onUp();
    }

private:
#pragma region handlers
    void(*m_onClick)(TContext&);
    void onClick() {
        if (m_onClick) m_onClick(m_context);
    }

    void(*m_onHold)(TContext&);
    void onHold() {
        if (m_onHold) m_onHold(m_context);
    }

    void(*m_onRelease)(TContext&);
    void onRelease() {
        if (m_onRelease) m_onRelease(m_context);
    }
#pragma endregion handlers

    void onStartHolding() {
        bwSet(state, IS_BEING_HELD, true);
        onHold();
    }

    void onDown() {
        downStartTime = millis();
        logger.info("button down");
    }

    void onUp() {
        logger.info("button up");
        bwSet(state, IS_HIGH, false);
        bwSet(state, IS_BEING_HELD, false);

        auto releaseTime = millis();
        if (getMillisDiff(releaseTime, downStartTime) <= BUTTON_HOLD_DIFF_MS) onClick();
        else onRelease();

    }
};

struct ServoRotator {
    static const uint16_t DEFAULT_OPEN_TIME_MS = 2000;
    static const uint8_t OPENED_DEGREES = 180;
    static const uint8_t CLOSED_DEGREES = 0;

    static const uint8_t IS_OPEN = 0b00000001;
    static const uint8_t IS_TIMED = 0b00000010;

    int m_pin = 0;

    uint32_t openedTime = 0;
    uint32_t closeTimePeriodMs = 0;

    Servo servo;
    uint8_t state = 0b00000000;

    explicit ServoRotator(const int pin): m_pin{pin} {
        servo.attach(pin);
        servo.write(CLOSED_DEGREES);
    }

    void open() {
        servo.write(OPENED_DEGREES);
        bwSet(state, IS_OPEN, true);
    }

    template<typename TTimePeriodMs = uint16_t> void openTimed(TTimePeriodMs timePeriodMs = DEFAULT_OPEN_TIME_MS) {
        open();
        bwSet(state, IS_TIMED, true);
        openedTime = millis();
        closeTimePeriodMs =  timePeriodMs;
    }

    void close() {
        logger.info("closing");
        bwSet(state, IS_OPEN, false);
        bwSet(state, IS_TIMED, false);
        servo.write(CLOSED_DEGREES);
    }

    void listen() {
        if (bwGet(state, IS_OPEN) && bwGet(state, IS_TIMED) && getMillisDiff(millis(), openedTime) >= closeTimePeriodMs) close();
    }
};

struct Program {
    Led redLed = Led{13};
    ServoRotator servoRotator{9};
    Button<Program> rotatorButton{
            2,
            *this,
            [](Program &program) {
                logger.debug("clicked");
                program.redLed.toggle();
                program.servoRotator.openTimed();
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

    void run() {
        rotatorButton.listen();
        servoRotator.listen();
    }
};

Program* program;

__attribute__((unused)) void setup() { program = new Program(); }
__attribute__((unused)) void loop() { program->run(); }