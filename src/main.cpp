#include <Arduino.h>

//begin DEFINITIONS
#define LED_PIN 13
#define BUTTON_PIN 2
#define BUTTON_HOLD_DIFF_MS 500
//end DEFINITIONS

struct Led {
    int pin;
    explicit Led(int pin): pin(pin) { pinMode(pin, OUTPUT); }

    void turnOn() const { digitalWrite(pin, HIGH); }
    void turnOff() const { digitalWrite(pin, LOW); }
    void toggle() const { digitalWrite(pin, !digitalRead(pin)); }
};

template<typename TContext>
struct Button {
    int m_pin = -1;
    uint32_t downStartTime = 0;
    int isDown = false;
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
        auto prevIsDown = isDown;
        isDown = digitalRead(m_pin);

        if (!prevIsDown && isDown) {
            onDown();
        } else if (prevIsDown && isDown && (millis() - downStartTime > BUTTON_HOLD_DIFF_MS)) {
            onHold();
        } else if (prevIsDown && !isDown) {
            onUp();
        }
    }

private:
    void(*m_onClick)(const TContext&);
    void(*m_onHold)(const TContext&);
    void(*m_onRelease)(const TContext&);

    void onClick() {
        if (m_onClick) m_onClick(m_context);
    }

    void onHold() {
        if (m_onHold) m_onHold(m_context);
    }

    void onRelease() {
        if (m_onRelease) m_onRelease(m_context);
    }

    void onDown() {
        downStartTime = millis();
        Serial.println("button down");
    }

    void onUp() {
        Serial.println("button up");
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