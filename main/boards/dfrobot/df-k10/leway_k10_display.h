#ifndef LEWAY_K10_DISPLAY_H
#define LEWAY_K10_DISPLAY_H

#include "display/lcd_display.h"
#include <lvgl.h>
#include <cstdint>
#include <functional>
#include <string>

/**
 * LewayK10Display — Interface LVGL 100% personalizada LeWay Presença para UNIHIKER K10 (240x320)
 *
 * Substitui o rostinho e o layout padrão do xiaozhi-esp32 por:
 * - Santuário Madeira Freijó (#2B1E16)
 * - Aura Orgânica LeWay de 3 anéis respirando/pulsando na cor de cada uma das 6 Presenças
 * - Olhos expressivos minimalistas com piscar orgânico natural e reações por emoção
 * - Nome da Presença em destaque + legenda estilo cinema limpa na base da tela
 * - Callback opcional para sincronizar os 3 LEDs RGB da UNIHIKER K10 com a cor da Presença
 */
class LewayK10Display : public SpiLcdDisplay {
public:
    using RgbColorCallback = std::function<void(uint8_t r, uint8_t g, uint8_t b)>;

    LewayK10Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                    int width, int height, int offset_x, int offset_y,
                    bool mirror_x, bool mirror_y, bool swap_xy);
    ~LewayK10Display() override;

    void SetRgbColorCallback(RgbColorCallback cb) { rgb_cb_ = std::move(cb); }

    void SetupUI() override;
    void SetStatus(const char* status) override;
    bool SetEmotion(const char* emotion) override;
    void SetChatMessage(const char* role, const char* content) override;
    void RemoveChatMessage(const char* role, const char* content) override;
    void ClearChatMessages() override;
    void SetPreviewImage(std::unique_ptr<LvglImage> image) override;
    void SetTheme(Theme* theme) override;
    void SetHideSubtitle(bool hide) override;

private:
    enum class VisualMood {
        Neutral,
        Happy,
        Laughing,
        Thinking,
        Surprised,
        Sad,
        Loving,
        Sleepy,
    };

    enum class VoiceState {
        Idle,
        Listening,
        Speaking,
    };

    struct PresenceTheme {
        const char* id;
        const char* display_name;
        const char* origin_hint;
        uint32_t accent_hex;
        uint8_t led_r;
        uint8_t led_g;
        uint8_t led_b;
    };

    // Elementos LVGL da tela LeWay (240x320)
    lv_obj_t* leway_root_ = nullptr;
    lv_obj_t* header_brand_label_ = nullptr;
    lv_obj_t* presence_name_label_ = nullptr;
    lv_obj_t* presence_sub_label_ = nullptr;

    // Aura orgânica (3 anéis concêntricos) + olhos expressivos
    lv_obj_t* aura_stage_ = nullptr;
    lv_obj_t* aura_outer_ = nullptr;
    lv_obj_t* aura_mid_ = nullptr;
    lv_obj_t* aura_core_ = nullptr;
    lv_obj_t* eye_left_ = nullptr;
    lv_obj_t* eye_right_ = nullptr;

    // Card de legenda limpa estilo cinema na base
    lv_obj_t* subtitle_card_ = nullptr;
    lv_obj_t* subtitle_role_label_ = nullptr;
    lv_obj_t* subtitle_text_label_ = nullptr;

    lv_timer_t* anim_timer_ = nullptr;

    // Estado de animação
    uint32_t accent_hex_ = 0xE69D45;
    VisualMood current_mood_ = VisualMood::Happy;
    VoiceState voice_state_ = VoiceState::Idle;
    uint32_t anim_tick_ = 0;
    uint32_t next_blink_tick_ = 55;
    bool is_blinking_ = false;
    RgbColorCallback rgb_cb_;

    static void AnimTimerCb(lv_timer_t* timer);
    void TickAnimation();
    void ApplyPresenceTheme(const std::string& presence_id);
    void ApplyMoodGeometry(bool blinking);
    void UpdateAuraSizes(int outer_d, int mid_d, int core_d, lv_opa_t outer_opa, lv_opa_t mid_opa);
};

#endif  // LEWAY_K10_DISPLAY_H
