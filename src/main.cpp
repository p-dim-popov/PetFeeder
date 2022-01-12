#include <Arduino.h>

#include <../.pio/libdeps/sparkfun_promicro16/Servo/src/Servo.h> // that include is bad...

// TODO: Separate functionalities into private libraries, pls soon...

#define getMillisDiff(ms, prevMs) ((unsigned long)((ms) - (prevMs)))

template<class T> struct IComparable { virtual int8_t compareTo(const T*) const = 0; };
template<class T> struct IEquatable { virtual bool equals(const T*) const = 0; };

struct {
    template<class TInput>
    static void println(const char level, const TInput arg, bool appendNewLine) {
        Serial.print(level);
        Serial.print("/");
        Serial.print(millis());
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

    template<class TInput>
    void info(const TInput arg, bool appendNewLine = true) const {
        if (level >= 3) println('i', arg, appendNewLine);
    }
    template<class TInput>
    void warn(const TInput arg, bool appendNewLine = true) const {
        if (level >= 2) println('w', arg, appendNewLine);
    }
    template<class TInput>
    void error(const TInput arg, bool appendNewLine = true) const {
        if (level >= 1) println('e', arg, appendNewLine);
    }
    template<class TInput>
    void debug(const TInput arg, bool appendNewLine = true) const {
        if (debugOn) println('d', arg, appendNewLine);
    }
} logger;

struct {
    template<typename TBus>
    bool get(TBus state, TBus stateBit) { return state & stateBit; }
    template<typename TBus>
    TBus set(TBus state, TBus stateBit, bool value) {
        return (value) ? state | stateBit : state & ~stateBit;
    }
    template<typename TBus, typename TIndex>
    TBus indexAsBit(TIndex index) {
        TBus result = 1;
        for (TIndex i = 0; i < index; i++) result *= 2;
        return result;
    }
} BitWise;

// TODO: better comparison implementation
struct Time: IEquatable<Time>, IComparable<Time> {
    uint8_t milliseconds = 0;
    uint8_t seconds = 0;
    uint8_t minutes = 0;
    uint8_t hours = 0;

    uint32_t toMs() const {
        return (hours * 3600000) + (minutes * 60000) + (seconds * 1000) + milliseconds;
    }

    bool equals(const Time* other) const override {
        if (other == nullptr) return false;
        return hours == other->hours
               && minutes == other->minutes
               && seconds == other->seconds
               && milliseconds == other->milliseconds;
    }

    int8_t compareTo(const Time* other) const override {
        if (other == nullptr) return 1;
        if (equals(other)) return 0;

        return (hours > other->hours
                 || (hours == other->hours && minutes > other->minutes)
                 || (hours == other->hours && minutes == other->minutes && seconds > other->seconds)
                 || (hours == other->hours && minutes == other->minutes && seconds == other->seconds && milliseconds > other->milliseconds))
                 ? 1 : -1;
    }

    bool isBetween(const Time& lowerBound, const Time& upperBound) const {
        const auto lowerBoundResult = lowerBound.compareTo(this);
        const auto upperBoundResult = upperBound.compareTo(this);
        return (lowerBoundResult >= 0) && (upperBoundResult >= 0);
    }

    static Time now() {
        auto currentMillis = millis();
        timeElapsed += getMillisDiff(currentMillis, prevMillis);
        prevMillis = currentMillis;
        return Time::fromMs(Time::started.toMs() + timeElapsed);
    }

    static Time fromMs(unsigned long long duration) {
        Time time = {};
        time.milliseconds = (duration % 1000) / 100;
        time.seconds = (duration / 1000) % 60;
        time.minutes = (duration / 60000) % 60;
        time.hours = (duration / 3600000) % 24;
        return time;
    }

    static void set(const Time &time) {
        started = time;
    }

private:
    static Time started;
    static uint32_t prevMillis;
    static uint32_t timeElapsed;
};

Time Time::started;
uint32_t Time::prevMillis = 0;
uint32_t Time::timeElapsed = 0;

struct IReact { virtual void react() = 0; };

template<typename TProps, typename TState>struct Component: IReact {
    TProps props;
    TState state;

    Component(): props{}, state{} {}

    virtual void componentDidUpdate(TProps prevProps, TState prevState) = 0;
    virtual TProps getUpdatedProps(bool& shouldUpdate) = 0;

    void setState(void(*transform)(TState& state)) {
        transform(_nextState);
        _shouldUpdate = true;
    }

    void setState(void(*transform)(TState& state, Component<TProps, TState>& context)) {
        transform(_nextState, *this);
        _shouldUpdate = true;
    }

    void react() override {
        _shouldUpdate = false;
        auto prevProps = props;
        props = getUpdatedProps(_shouldUpdate);
        if (!_shouldUpdate) return;
        do {
            _shouldUpdate = false;
            auto prevState = state;
            state = _nextState;
            componentDidUpdate(prevProps, prevState);
            prevProps = props;
        } while(_shouldUpdate);

        state = _nextState;
    }

private:
    bool _shouldUpdate = false;
    TState _nextState;
};

template<typename TContext> struct DayJob: IEquatable<DayJob<TContext>> {
    const Time time;
    void(*task)(TContext&);
    explicit DayJob(const Time time, void(*task)(TContext&)): time{time}, task{task} {}

    bool equals(const DayJob<TContext>* other) const override { return time.equals(&other->time); }
};

template<typename TItem, unsigned short size> struct Array {
    TItem* list[size]{};
    unsigned short count = 0;

    virtual bool add(TItem *item) {
        if (count == size) return false;

        list[count++] = item;
        return true;
    }

    bool removeAt(int index) {
        if (!count || index < 0 || index >= count) return false;
        for (unsigned short i = index; i < count; ++i) {
            list[i] = at(i + 1);
        }
        count--;
        return true;
    }

    TItem* at(int index) {
        if (index < 0 || index >= count) return nullptr;

        return list[index];
    }

    long indexOf(const IEquatable<TItem>* item) {
        for (unsigned short i = 0; i < count; ++i) {
            const auto currentItem = list[i];
            if (item->equals(currentItem)) return i;
        }
        return -1;
    }

    TItem* find(IEquatable<TItem>* item) {
        for (unsigned int i = 0; i < count; ++i) {
            const auto current = list[i];
            if (item->equals(current)) return current;
        }

        return nullptr;
    }
};

template<typename TItem, unsigned int size> struct Set: Array<TItem, size> {
    bool add(TItem *item) override {
        if (Array<TItem, size>::count == size) return false;
        if (Array<TItem, size>::find(item)) return false;

        return Array<TItem, size>::add(item);
    }
};

template<typename TContext> struct DayJobsScheduler: IReact {
    static const unsigned char MAX_JOBS = 10;

    explicit DayJobsScheduler(TContext& context): _context(context) {}

    void react() override {
        const auto timeNow = Time::now();
        const auto oneMinuteAfterTimeNow = Time::fromMs(timeNow.toMs() + 60000);

        for (unsigned int i = 0; i < _jobs.count; ++i) {
            const auto job = _jobs.at(i);
            const auto stateBit = BitWise.indexAsBit<uint32_t>(i);
            const auto isDone = BitWise.get(_checkList, stateBit);
            const auto isTimeForDoingTheJob = job->time.isBetween(timeNow, oneMinuteAfterTimeNow);
            if (isTimeForDoingTheJob && !isDone) {
                    job->task(_context);
                    _checkList = BitWise.set(_checkList, stateBit, true);
            } else if (!isTimeForDoingTheJob && isDone) _checkList = BitWise.set(_checkList, stateBit, false);
        }
    }

    bool schedule(DayJob<TContext>* job) {
        return _jobs.add(job);
    }

    bool unschedule(DayJob<TContext>* job) {
        const auto index = _jobs.indexOf(job);
        return unschedule(index);
    }
    bool unschedule(int index) {
        const auto prevCount = _jobs.count;
        if (!_jobs.removeAt(index)) return false;

        for (auto i = index; i < prevCount; ++i)
            _checkList = BitWise.set(
                    _checkList,
                    BitWise.indexAsBit<uint32_t>(i),
                    ((i + 1) < prevCount) && BitWise.get(_checkList, BitWise.indexAsBit<uint32_t>(i + 1)
                    ));
        return true;
    }

private:
    TContext& _context;
    uint32_t _checkList = 0b0;
    Set<DayJob<TContext>, MAX_JOBS> _jobs;
};

struct Led {
    int _pin;
    explicit Led(int pin): _pin(pin) { pinMode(pin, OUTPUT); }

    bool isOn() const { return digitalRead(_pin); }
    void turnOn() const { digitalWrite(_pin, HIGH); }
    void turnOff() const { digitalWrite(_pin, LOW); }
    void toggle() const { digitalWrite(_pin, !isOn()); }
};

struct ButtonProps {
    unsigned long millis = 0;
    bool isHigh = false;
};

struct ButtonState {
    unsigned long downStartTime = 0;
    bool isBeingHeld = false;
    bool isHigh = false;
    bool shouldCheckDownStartTime = false;
};

template<typename TContext> struct Button : Component<ButtonProps, ButtonState> {
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

     ButtonProps getUpdatedProps(bool& shouldUpdate) override {
        auto nextProps = props;

        const auto currentMillis = millis();
         if (getMillisDiff(currentMillis, props.millis)) {
             nextProps.millis = currentMillis;
             shouldUpdate = true;
         }

         const auto currentIsHigh = digitalRead(_pin);
         if (currentIsHigh != props.isHigh) {
             nextProps.isHigh = currentIsHigh;
             shouldUpdate = true;
         }

        return nextProps;
    }

    void componentDidUpdate(ButtonProps prevProps, ButtonState prevState) override {
        if (prevProps.isHigh != props.isHigh) {
            if (props.isHigh) {
                logger.debug("start down");
                this->setState([](ButtonState& nextState, Component<ButtonProps, ButtonState>& context){
                    nextState.downStartTime = context.props.millis;
                });
            } else {
                this->setState([](ButtonState& nextState){
                    nextState.isHigh = false;
                });
                logger.debug("end down");
            }
        }

        if (prevState.downStartTime != state.downStartTime) {
            logger.info("starting to check down time");
            this->setState([](ButtonState& nextState){
                nextState.shouldCheckDownStartTime = true;
            });
        }

        if (state.shouldCheckDownStartTime && !state.isHigh && props.isHigh) {
            if (getMillisDiff(props.millis, state.downStartTime) > BUTTON_CLICK_DIFF_MS) {
                this->setState([](ButtonState& nextState){
                    nextState.isHigh = true;
                });
            }
        }

        if (state.isHigh && (getMillisDiff(props.millis, state.downStartTime) > BUTTON_HOLD_DIFF_MS) && !state.isBeingHeld) {
            this->setState([](ButtonState& nextState){
                nextState.isBeingHeld = true;
            });
            onHold();
        }

        if (prevState.isHigh && !state.isHigh) {
            logger.info("button up");
            this->setState([](ButtonState& nextState){
                nextState.isHigh = false;
                nextState.isBeingHeld = false;
                nextState.shouldCheckDownStartTime = false;
            });

            if (getMillisDiff(props.millis, state.downStartTime) < BUTTON_HOLD_DIFF_MS) onClick();
            else onRelease();
        }
    }

private:
    static const uint16_t BUTTON_HOLD_DIFF_MS = 500;
    static const uint16_t BUTTON_CLICK_DIFF_MS = 50;

    int _pin = -1;

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
};

struct ServoRotator: IReact {
    explicit ServoRotator(const int pin) {
        _servo.attach(pin);
        _servo.write(CLOSED_DEGREES);
    }

    void open() {
        _servo.write(OPENED_DEGREES);
        state = BitWise.set(state, IS_OPEN, true);
    }

    template<typename TTimePeriodMs = uint16_t> void openTimed(TTimePeriodMs timePeriodMs = DEFAULT_OPEN_TIME_MS) {
        open();
        state = BitWise.set(state, IS_TIMED, true);
        _openedTime = millis();
        _closeTimePeriodMs =  timePeriodMs;
    }

    void close() {
        logger.info("closing");
        state = BitWise.set(state, IS_OPEN, false);
        state = BitWise.set(state, IS_TIMED, false);
        _servo.write(CLOSED_DEGREES);
    }

    void react() override {
        if (BitWise.get(state, IS_OPEN) && BitWise.get(state, IS_TIMED) && getMillisDiff(millis(), _openedTime) >= _closeTimePeriodMs) close();
    }

private:
    static const uint16_t DEFAULT_OPEN_TIME_MS = 2000;
    static const uint8_t OPENED_DEGREES = 180;
    static const uint8_t CLOSED_DEGREES = 0;

    static const uint8_t IS_OPEN = 0b00000001;
    static const uint8_t IS_TIMED = 0b00000010;

    Servo _servo;
    uint8_t state = 0b00000000;
    uint32_t _openedTime = 0;
    uint32_t _closeTimePeriodMs = 0;
};

struct Program {
    DayJob<Program> testJob{Time::fromMs(Time::now().toMs() + 5000), [](Program& program){ program.redLed.turnOn(); }};
    const Led redLed{13};
    ServoRotator servoRotator{9};
    Button<Program> rotatorButton{
            2,
            *this,
            [](Program &program) {
                logger.debug("clicked");
                program.redLed.toggle();
                program.servoRotator.openTimed();
                // Time::set(Time::fromMs(1641669327069)); // 19:15:28
                if (program.jobsScheduler.schedule(&program.testJob)) logger.info("scheduled");
            },
            [](Program &program) {
                logger.debug("held");
                program.redLed.turnOn();
                program.servoRotator.open();
                if (program.jobsScheduler.unschedule(&program.testJob)) logger.info("unscheduled");
            },
            [](Program &program) {
                logger.debug("released");
                program.redLed.turnOff();
                program.servoRotator.close();
            }
    };
    DayJobsScheduler<Program> jobsScheduler{*this};

    Program() {
        Serial.begin(9600);
    }

    void act() {
        rotatorButton.react();
        servoRotator.react();
        jobsScheduler.react();
        auto time = Time::now();
        String res;
        res.concat(time.hours);
        res.concat(":");
        res.concat(time.minutes);
        res.concat(":");
        res.concat(time.seconds);
        static String x;
        if (x != res) {
            x = res;
            logger.debug(res.c_str());
        }
    }
};

Program* program;

__attribute__((unused)) void setup() { program = new Program(); }
__attribute__((unused)) void loop() { program->act(); }
