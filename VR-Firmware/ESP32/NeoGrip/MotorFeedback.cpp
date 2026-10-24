#include "MotorFeedback.h"
#include "esp_timer.h"

// Keep ISR definitions out of inline C++ header sections so Xtensa places their
// literal pools with the IRAM code. No allocation, logging or PWM driver calls.
void MotorFeedback::driveOffLocked() {
    gpio_ll_set_level(&GPIO, pin_, offLevel_);
    esp_rom_gpio_connect_out_signal(pin_, SIG_GPIO_OUT_IDX, false, false);
    gpio_ll_output_enable(&GPIO, pin_);
    on_ = false;
}

bool MotorFeedback::safetyInterrupt(gptimer_handle_t, const gptimer_alarm_event_data_t*, void* argument) {
    auto* self = static_cast<MotorFeedback*>(argument);
    const uint32_t nowUs = uint32_t(esp_timer_get_time());
    portENTER_CRITICAL_ISR(&self->lock_);
    if (self->on_ && int32_t(nowUs - self->pulseDeadlineUs_) >= 0) {
        self->driveOffLocked();
    }
    portEXIT_CRITICAL_ISR(&self->lock_);
    return false;
}
