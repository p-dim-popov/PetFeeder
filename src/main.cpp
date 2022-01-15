#include <Arduino.h>

#include <../.pio/libdeps/sparkfun_promicro16/Servo/src/Servo.h> // that include is bad...

// TODO: Separate functionalities into private libraries, pls soon...

#define getMillisDiff(ms, prevMs) ((unsigned long)((ms) - (prevMs)))

template<typename T> struct IComparable { virtual int8_t compareTo(const T*) const = 0; };
template<typename T> struct IEquatable { virtual bool equals(const T*) const = 0; };

/**
 * Levels:
 *  - no logs 0
 *  - error 1
 *  - warn 2
 *  - info 3
 *  - debug
 */
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

template<typename TProps, typename TState>
struct Component: IReact {
    TProps props;
    TState state;

    Component(): props{}, state{} {}

    virtual void componentDidUpdate(const TProps& prevProps, const TState& prevState, TState& nextState, bool& shouldUpdate) = 0;
    virtual void getUpdatedProps(TProps& nextProps, bool& shouldUpdate) = 0;

    void react() override {
        bool shouldUpdate = false;
        auto prevProps = props;
        getUpdatedProps(props, shouldUpdate);
        auto nextState = state;
        while (shouldUpdate) {
            shouldUpdate = false;
            auto prevState = state;
            state = nextState;
            componentDidUpdate(prevProps, prevState, nextState, shouldUpdate);
            prevProps = props;
        }
    }
};

template<typename TContext> struct DayJob: IEquatable<DayJob<TContext>> {
    const Time time;
    void(*task)(TContext&);
    explicit DayJob(const Time time, void(*task)(TContext&)): time{time}, task{task} {}

    bool equals(const DayJob<TContext>* other) const override { return time.equals(&other->time); }
};

template<typename TItem, uint8_t SIZE> struct StaticArray {
    TItem list[SIZE]{};
    uint8_t count = 0;
    uint8_t size = SIZE;

    virtual bool add(TItem item) {
        if (count == SIZE) return false;

        list[count++] = item;
        return true;
    }

    const TItem* at(int16_t index) const {
        if (index < 0 || index >= count) return nullptr;

        return &(list[index]);
    }

    bool set(int16_t index, TItem item) {
        if (index < 0 || index >= count) return false;
        list[index] = item;
        return true;
    }
};

template<typename TItem, unsigned short SIZE> struct Array {
    TItem* list[SIZE]{};
    unsigned short count = 0;
    const unsigned short size = SIZE;

    Array() = default;
    Array(unsigned short count, TItem* items...): list{items}, count{count} {}
    explicit Array(TItem* init[SIZE]): list{init} { for (int i = 0; i < SIZE && init[i]; ++i) count = i; }

    virtual bool add(TItem *item) {
        if (count == SIZE) return false;

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

    TItem* at(int index) const {
        if (index < 0 || index >= count) return nullptr;

        return list[index];
    }

    bool set(int index, TItem* item) {
        if (index < 0 || index >= count) return false;
        list[index] = item;
        return true;
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

    TItem* begin(){return list[0];}
    TItem* end(){return begin() + count;}
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

template<uint8_t bufferSize>
struct StreamListenerProps {
};

template<uint8_t bufferSize>
struct StreamListenerState {
    StaticArray<char, bufferSize> buffer{};
    bool shouldSendData = false;
};

template <typename TContext, uint8_t bufferSize = 20>
struct StreamListener: Component<StreamListenerProps<bufferSize>, StreamListenerState<bufferSize>> {
    StreamListener(
            TContext& context,
            Stream& stream,
            void(*onInput)(TContext&, const char*),
            const char* terminatingCharacters = "\r\n"
        ):
        _context{context},
        _stream{stream},
        _terminatingCharacters{terminatingCharacters},
        _onInput{onInput}
        {}

    void getUpdatedProps(StreamListenerProps<bufferSize>& nextProps, bool& shouldUpdate) override {
        shouldUpdate = _stream.available() > 0;
    }

    void componentDidUpdate(
            const StreamListenerProps<bufferSize>& prevProps,
            const StreamListenerState<bufferSize>& prevState,
            StreamListenerState<bufferSize>& nextState,
            bool& shouldUpdate
        ) override {
        if (_stream.available() > 0) {
            bool hasBeenTerminated = false;

            while (_stream.available() > 0) {
                const char currentChar = _stream.read();

                auto iter = _terminatingCharacters;
                while (*iter) if (currentChar == *(iter++)) hasBeenTerminated = true;
                if (hasBeenTerminated) {
                    break;
                }

                const auto isAddSuccess = nextState.buffer.add(currentChar);
                if (!isAddSuccess) {
                    break;
                }
            }

            if (this->state.buffer.count == this->state.buffer.size || hasBeenTerminated) {
                if (this->state.buffer.count == this->state.buffer.size) {
                    // TODO: empty the buffer maybe? undefined behaviour?
                    nextState.buffer.set(nextState.buffer.size - 1, '\0');
                } else if (hasBeenTerminated) {
                    if (! nextState.buffer.add('\0')) nextState.buffer.set(nextState.buffer.size - 1, '\0');
                }

                nextState.shouldSendData = true;
            }

            shouldUpdate = true;
        }

        if (this->state.shouldSendData) {
            nextState.shouldSendData = false;
            onInput((const char *)(this->state.buffer.list));
            nextState.buffer = StaticArray<char, bufferSize>{};
            shouldUpdate = true;
        }
    }

private:
    TContext& _context;
    Stream& _stream;
    const char* _terminatingCharacters;

    void (*_onInput)(TContext&, const char*);
    void onInput(const char* value) { if (_onInput) _onInput(_context, value); }
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

     void getUpdatedProps(ButtonProps& nextProps, bool& shouldUpdate) override {
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

    }

    void componentDidUpdate(
            const ButtonProps &prevProps,
            const ButtonState &prevState,
            ButtonState &nextState,
            bool &shouldUpdate
        ) override {
        if (prevProps.isHigh != props.isHigh) {
            if (props.isHigh) {
                logger.debug("start down");
                nextState.downStartTime = props.millis;
            } else {
                nextState.isHigh = false;
                logger.debug("end down");
            }

            shouldUpdate = true;
        }

        if (prevState.downStartTime != state.downStartTime) {
            logger.info("starting to check down time");
            nextState.shouldCheckDownStartTime = true;
            shouldUpdate = true;
        }

        if (state.shouldCheckDownStartTime && !state.isHigh && props.isHigh) {
            if (getMillisDiff(props.millis, state.downStartTime) > BUTTON_CLICK_DIFF_MS) {
                nextState.isHigh = true;
                shouldUpdate = true;
            }
        }

        if (state.isHigh && (getMillisDiff(props.millis, state.downStartTime) > BUTTON_HOLD_DIFF_MS) && !state.isBeingHeld) {
            nextState.isBeingHeld = true;
            shouldUpdate = true;
            onHold();
        }

        if (prevState.isHigh && !state.isHigh) {
            logger.info("button up");
            nextState.isHigh = false;
            nextState.isBeingHeld = false;
            nextState.shouldCheckDownStartTime = false;
            shouldUpdate = true;

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
    StreamListener<Program, 20> streamListener{
        *this,
        Serial,
        [](Program& program, const char* input){
            logger.debug("received:");
            logger.debug(input);
        },
    };
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
        streamListener.react();
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
