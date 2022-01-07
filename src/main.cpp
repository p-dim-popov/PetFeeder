#include <Arduino.h>

// TODO: Separate functionalities into private libraries

//begin DEFINITIONS
#define LED_PIN 13
#define BUTTON_PIN 2
#define BUTTON_HOLD_DIFF_MS 500
//end DEFINITIONS

#define GET(state, x) (bool)(state & x)
#define SET(state, x, value) (value ? state |= x : state &= ~x)

struct Led {
    int pin;
    explicit Led(int pin): pin(pin) { pinMode(pin, OUTPUT); }

    void turnOn() const { digitalWrite(pin, HIGH); }
    void turnOff() const { digitalWrite(pin, LOW); }
    void toggle() const { digitalWrite(pin, !digitalRead(pin)); }
};

template<typename TContext>
struct Button {
    static const uint8_t IS_HIGH = 0b00000001;
    static const uint8_t IS_BEING_HELD = 0b00000010;

    int m_pin = -1;
    uint32_t downStartTime = 0;
    uint8_t state = 0b00000000;
    TContext& m_context;

    explicit Button(
            int pin,
            TContext& context,
            void(*onClick)(const TContext&) = nullptr,
            void(*onHold)(const TContext&) = nullptr,
            void(*onRelease)(const TContext&) = nullptr
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
        auto prevIsHigh = GET(state, IS_HIGH);
        auto isHigh = GET(SET(state, IS_HIGH, digitalRead(m_pin)), IS_HIGH);

        if (!prevIsHigh && isHigh) {
            onDown();
        } else if (prevIsHigh && isHigh && !GET(state, IS_BEING_HELD) && (millis() - downStartTime > BUTTON_HOLD_DIFF_MS)) {
            onStartHolding();
        } else if (prevIsHigh && !isHigh) {
            onUp();
        }
    }

private:
#pragma region handlers
    void(*m_onClick)(const TContext&);
    void onClick() {
        if (m_onClick) m_onClick(m_context);
    }

    void(*m_onHold)(const TContext&);
    void onHold() {
        if (m_onHold) m_onHold(m_context);
    }

    void(*m_onRelease)(const TContext&);
    void onRelease() {
        if (m_onRelease) m_onRelease(m_context);
    }
#pragma endregion handlers

    void onStartHolding() {
        SET(state, IS_BEING_HELD, true);
        onHold();
    }

    void onDown() {
        downStartTime = millis();
        Serial.println("button down");
    }

    void onUp() {
        Serial.println("button up");
        SET(state, IS_HIGH, false);
        SET(state, IS_BEING_HELD, false);

        auto releaseTime = millis();
        if (releaseTime - downStartTime <= BUTTON_HOLD_DIFF_MS) onClick();
        else onRelease();

    }
};

struct Program {
    Led led13 = Led(LED_PIN);
    Button<Program> button2 = Button<Program>(
        BUTTON_PIN,
        *this,
        [](const Program &program) {
            Serial.println("clicked");
            program.led13.toggle();
        },
        [](const Program& program){
            Serial.println("held");
            program.led13.turnOn();
        },
        [](const Program& program){
            Serial.println("released");
            program.led13.turnOff();
        }
    );

    Program() {
        Serial.begin(9600);
    }

    void run() {
        button2.listen();
    }
};

Program* program;

__attribute__((unused)) void setup() { program = new Program(); }
__attribute__((unused)) void loop() { program->run(); }