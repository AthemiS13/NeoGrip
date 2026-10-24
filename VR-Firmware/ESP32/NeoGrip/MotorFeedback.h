#pragma once
#include <Arduino.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/gptimer.h"
#include "esp_rom_gpio.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_sig_map.h"
#include "soc/gpio_struct.h"

enum class FeedbackEvent : uint8_t {
    None, Wake, WiFiReady, Connected, WiFiLost, ProxyLost, Setup, Sleep, Restart
};

class MotorFeedback {
public:
    MotorFeedback(int pin, bool activeLow) : pin_(pin), offLevel_(activeLow ? 1 : 0), activeLow_(activeLow) {}

    bool begin() {
        // Establish the output latch before enabling/releasing the pin. Do not
        // release a retained pad into the reset-default low level on an active-low driver.
        gpio_set_level((gpio_num_t)pin_, offLevel_);
        gpio_set_direction((gpio_num_t)pin_, GPIO_MODE_INPUT_OUTPUT);
        driveOffLocked();
        gpio_hold_dis((gpio_num_t)pin_);
        gpio_deep_sleep_hold_dis();
        gpio_set_pull_mode((gpio_num_t)pin_, activeLow_ ? GPIO_PULLUP_ONLY : GPIO_PULLDOWN_ONLY);

        ledc_timer_config_t timerConfig = {};
        timerConfig.speed_mode = LEDC_LOW_SPEED_MODE;
        timerConfig.duty_resolution = LEDC_TIMER_8_BIT;
        timerConfig.timer_num = LEDC_TIMER_0;
        timerConfig.freq_hz = 200;
        timerConfig.clk_cfg = LEDC_AUTO_CLK;
        if (ledc_timer_config(&timerConfig) != ESP_OK) return false;
        if (gpio_hold_en((gpio_num_t)pin_) != ESP_OK) return false;
        ledc_channel_config_t channelConfig = {};
        channelConfig.gpio_num = pin_;
        channelConfig.speed_mode = LEDC_LOW_SPEED_MODE;
        channelConfig.channel = LEDC_CHANNEL_0;
        channelConfig.timer_sel = LEDC_TIMER_0;
        channelConfig.intr_type = LEDC_INTR_DISABLE;
        channelConfig.duty = activeLow_ ? 256 : 0;
        const esp_err_t result = ledc_channel_config(&channelConfig);
        driveOffLocked();
        gpio_hold_dis((gpio_num_t)pin_);
        if (result != ESP_OK) return false;

        gptimer_config_t safetyConfig = {};
        safetyConfig.clk_src = GPTIMER_CLK_SRC_DEFAULT;
        safetyConfig.direction = GPTIMER_COUNT_UP;
        safetyConfig.resolution_hz = 1000000;
        if (gptimer_new_timer(&safetyConfig, &safetyTimer_) != ESP_OK) return false;
        gptimer_event_callbacks_t callbacks = {};
        callbacks.on_alarm = safetyInterrupt;
        gptimer_alarm_config_t alarm = {};
        alarm.alarm_count = 5000;
        alarm.reload_count = 0;
        alarm.flags.auto_reload_on_alarm = true;
        if (gptimer_register_event_callbacks(safetyTimer_, &callbacks, this) != ESP_OK ||
            gptimer_set_alarm_action(safetyTimer_, &alarm) != ESP_OK ||
            gptimer_enable(safetyTimer_) != ESP_OK || gptimer_start(safetyTimer_) != ESP_OK) return false;
        ready_ = true;
        return true;
    }

    void gamePulse(int strength, uint32_t durationMs, uint32_t now) {
        if (!ready_ || inhibited_ || terminal_ || event_ != FeedbackEvent::None) return;
        if (strength <= 0 || durationMs == 0) {
            gameActive_ = false;
            off();
            return;
        }
        gameStrength_ = constrain(strength, 0, 255);
        gameDurationMs_ = constrain(durationMs, uint32_t(20), maxCommandMs);
        gameStartedAt_ = now;
        gameActive_ = true;
    }

    void notify(FeedbackEvent event, uint32_t now) {
        if (!ready_ || inhibited_ || event == FeedbackEvent::None) return;
        if (terminal_ && event != FeedbackEvent::Sleep && event != FeedbackEvent::Restart) return;
        // Shutdown/restart can preempt other cues; ordinary repeated events do not restart a cue.
        if (event_ == FeedbackEvent::Sleep || event_ == FeedbackEvent::Restart) return;
        if (event_ == event) return;
        if (event == FeedbackEvent::Sleep || event == FeedbackEvent::Restart) terminal_ = true;
        const bool urgent = event == FeedbackEvent::Sleep || event == FeedbackEvent::Restart ||
                            event == FeedbackEvent::WiFiLost || event == FeedbackEvent::ProxyLost;
        if (event_ != FeedbackEvent::None && !urgent) { pendingEvent_ = event; return; }
        gameActive_ = false;
        off();
        pendingEvent_ = FeedbackEvent::None;
        event_ = event;
        eventStartedAt_ = now;
    }

    void service(uint32_t now) {
        if (!ready_ || inhibited_) return;
        if (event_ != FeedbackEvent::None) {
            if (int32_t(now - eventStartedAt_) < 0) { off(); return; }
            const Pattern pattern = patternFor(event_);
            uint32_t elapsed = uint32_t(now - eventStartedAt_);
            for (uint8_t i = 0; i < pattern.count; ++i) {
                const Segment& segment = pattern.segments[i];
                if (elapsed < segment.durationMs) {
                    if (!segment.strength) off();
                    else apply(envelope(segment.strength, elapsed, segment.durationMs), segment.durationMs - elapsed);
                    return;
                }
                elapsed -= segment.durationMs;
            }
            event_ = FeedbackEvent::None;
            off();
            if (pendingEvent_ != FeedbackEvent::None) {
                event_ = pendingEvent_; pendingEvent_ = FeedbackEvent::None;
                eventStartedAt_ = now + 150;
                return;
            }
        }
        if (gameActive_) {
            const uint32_t elapsed = uint32_t(now - gameStartedAt_);
            if (elapsed < gameDurationMs_) apply(gameStrength_, gameDurationMs_ - elapsed);
            else { gameActive_ = false; off(); }
        }
    }

    bool notificationActive() const { return ready_ && event_ != FeedbackEvent::None; }

    void cancel() {
        event_ = FeedbackEvent::None;
        pendingEvent_ = FeedbackEvent::None;
        gameActive_ = false;
        off();
    }

    void stopGame() {
        gameActive_ = false;
        if (event_ == FeedbackEvent::None) off();
    }

    bool prepareStandby() {
        cancel();
        inhibited_ = true;
        if (safetyTimer_ && ready_) {
            gptimer_stop(safetyTimer_);
            gptimer_disable(safetyTimer_);
        }
        // PWM must no longer own the physical pad before latching it for sleep.
        gpio_set_direction((gpio_num_t)pin_, GPIO_MODE_INPUT_OUTPUT);
        off();
        if (gpio_get_level((gpio_num_t)pin_) != offLevel_) return false;
        return gpio_hold_en((gpio_num_t)pin_) == ESP_OK;
    }

    void resume() {
        // Program the off state while the pad is still held, then release it.
        gpio_set_level((gpio_num_t)pin_, offLevel_);
        gpio_set_direction((gpio_num_t)pin_, GPIO_MODE_INPUT_OUTPUT);
        off();
        gpio_hold_dis((gpio_num_t)pin_);
        gpio_deep_sleep_hold_dis();
        inhibited_ = false;
        terminal_ = false;
        if (safetyTimer_ && ready_ &&
            (gptimer_enable(safetyTimer_) != ESP_OK || gptimer_start(safetyTimer_) != ESP_OK)) ready_ = false;
    }

    // One transport segment, not a game-effect limit. The proxy renews longer
    // effects before this expires; missing renewals always let the motor stop.
    static constexpr uint32_t maxCommandMs = 999;

private:
    struct Segment { uint16_t durationMs; uint8_t strength; };
    struct Pattern { const Segment* segments; uint8_t count; };
    int pin_;
    int offLevel_;
    bool activeLow_;
    bool ready_ = false;
    bool inhibited_ = false;
    bool terminal_ = false;
    gptimer_handle_t safetyTimer_ = nullptr;
    portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
    volatile bool on_ = false;
    volatile uint32_t pulseDeadlineUs_ = 0;
    FeedbackEvent event_ = FeedbackEvent::None;
    FeedbackEvent pendingEvent_ = FeedbackEvent::None;
    uint32_t eventStartedAt_ = 0;
    bool gameActive_ = false;
    int gameStrength_ = 0;
    uint32_t gameStartedAt_ = 0, gameDurationMs_ = 0;

    void IRAM_ATTR driveOffLocked();
    static bool IRAM_ATTR safetyInterrupt(gptimer_handle_t, const gptimer_alarm_event_data_t*, void* argument);

    void off() {
        portENTER_CRITICAL(&lock_);
        driveOffLocked();
        portEXIT_CRITICAL(&lock_);
    }

    void apply(int strength, uint32_t remainingMs) {
        if (!ready_ || inhibited_ || !remainingMs) { off(); return; }
        const uint32_t nowUs = micros();
        portENTER_CRITICAL(&lock_);
        pulseDeadlineUs_ = nowUs + (remainingMs > maxCommandMs ? maxCommandMs : remainingMs) * 1000;
        on_ = true;
        portEXIT_CRITICAL(&lock_);

        const uint32_t duty = activeLow_ ? 255 - constrain(strength, 0, 255) : constrain(strength, 0, 255);
        if (ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty) != ESP_OK ||
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0) != ESP_OK) { off(); return; }

        // Reconnect PWM only if the ISR has not expired this pulse while the
        // task was preempted. The duty calls above never reconnect the pad.
        portENTER_CRITICAL(&lock_);
        if (on_ && int32_t(micros() - pulseDeadlineUs_) < 0) {
            esp_rom_gpio_connect_out_signal(pin_, LEDC_LS_SIG_OUT0_IDX, false, false);
        } else driveOffLocked();
        portEXIT_CRITICAL(&lock_);
    }

    static int envelope(int peak, uint32_t elapsed, uint32_t duration) {
        // A short spin-up shoulder, rounded attack and a longer release avoid a
        // full-power click while giving an ERM motor time to become perceptible.
        if (elapsed < 25) return 165 + (peak - 165) * int(elapsed) / 25;
        const uint32_t remaining = duration - elapsed;
        if (remaining < 45) return 70 + (peak - 70) * int(remaining) / 45;
        return peak;
    }

    static Pattern patternFor(FeedbackEvent event) {
        static const Segment wake[] = {{160,185},{110,0},{180,215}};
        static const Segment wifi[] = {{130,190}};
        static const Segment connected[] = {{90,190},{85,0},{90,190},{85,0},{170,215}};
        static const Segment wifiLost[] = {{170,205},{170,0},{170,205},{170,0},{170,205}};
        static const Segment proxyLost[] = {{210,205},{210,0},{210,205}};
        static const Segment setup[] = {{180,200},{100,0},{90,185},{100,0},{180,200}};
        static const Segment sleep[] = {{170,195},{120,0},{120,165}};
        static const Segment restart[] = {{90,200},{80,0},{90,200},{80,0},{90,200}};
        switch (event) {
            case FeedbackEvent::Wake: return {wake,3};
            case FeedbackEvent::WiFiReady: return {wifi,1};
            case FeedbackEvent::Connected: return {connected,5};
            case FeedbackEvent::WiFiLost: return {wifiLost,5};
            case FeedbackEvent::ProxyLost: return {proxyLost,3};
            case FeedbackEvent::Setup: return {setup,5};
            case FeedbackEvent::Sleep: return {sleep,3};
            case FeedbackEvent::Restart: return {restart,5};
            default: return {nullptr,0};
        }
    }
};
