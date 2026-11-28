#ifndef LEWAY_K10_DISPLAY_H
#define LEWAY_K10_DISPLAY_H

// 1 = Horizontal Puro Só Olhos Vivos (320x240: zero barra/Wi-Fi/hora/texto, olhos grandes expressivos)
// 0 = Vertical Clássico (240x320: LeWay OS + Nome + Aura + Legenda compacta)
#ifndef LEWAY_LANDSCAPE_EYES
#define LEWAY_LANDSCAPE_EYES 1
#endif

#include "display/lcd_display.h"
#include <lvgl.h>
#include <cstdint>
#include <functional>
#include <string>

/**
 * LewayK10Display — Interface LVGL 100% personalizada LeWay OS para UNIHIKER K10
 *
 * Modos selecionáveis em compilação (LEWAY_LANDSCAPE_EYES):
 * - 1 (Horizontal 320x240 — Olhos Vivos): Tela limpa absoluta (sem topo, Wi-Fi, relógio ou texto),
 *   com dois olhos grandes, meigos e expressivos (duplo brilho especular com paralaxe, pálpebras
 *   superiores/inferiores para sorriso curvo, bochechas rosadas de afeto, olhar espontâneo pros lados,
 *   despertar animado com pulinho, sono progressivo em silêncio e cara emburrada em erro).
 * - 0 (Vertical 240x320 — Clássico): Paleta Freijó (#2B1E16), topo "LeWay OS", Nome da Presença,
 *   Aura orgânica de 3 anéis e legenda compacta fiel ao estado (FALANDO / OUVINDO / EM ESPERA).
 */
class LewayK10Display : public SpiLcdDisplay {
public:
    using RgbColorCallback = std::function<void(uint8_t r, uint8_t g, uint8_t b)>;
    using LampModeCallback = std::function<void(bool enabled, uint8_t r, uint8_t g, uint8_t b)>;

    LewayK10Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                    int width, int height, int offset_x, int offset_y,
                    bool mirror_x, bool mirror_y, bool swap_xy);
    ~LewayK10Display() override;

    void SetRgbColorCallback(RgbColorCallback cb) { rgb_cb_ = std::move(cb); }
    void SetLampModeCallback(LampModeCallback cb) { lamp_cb_ = std::move(cb); }

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
        Grumpy,
    };

    enum class VoiceState {
        Idle,
        Listening,
        Speaking,
    };

    struct PresenceTheme {
        const char* id;
        const char* display_name;
        uint32_t outer_hex;
        uint32_t mid_hex;
        uint32_t core_hex;
        uint8_t led_r;
        uint8_t led_g;
        uint8_t led_b;
    };

    struct EyeFrameCache {
        int lw = -1, lh = -1, lx = 999, ly = 999, lrad = -1;
        int rw = -1, rh = -1, rx = 999, ry = 999, rrad = -1;
        int l_top_h = -1, r_top_h = -1;
        int l_bot_h = -1, r_bot_h = -1;
        int gaze_x = 999, gaze_y = 999;
        bool glint_vis = true;
        bool cheeks_vis = false;
        int glint_big = -1;
    };

    // Elementos LVGL
    lv_obj_t* leway_root_ = nullptr;
    lv_obj_t* header_brand_label_ = nullptr;
    lv_obj_t* presence_name_label_ = nullptr;

    // Aura orgânica (vertical) + olhos vivos expressivos (horizontal e vertical)
    lv_obj_t* aura_stage_ = nullptr;
    lv_obj_t* aura_outer_ = nullptr;
    lv_obj_t* aura_mid_ = nullptr;
    lv_obj_t* aura_core_ = nullptr;

    lv_obj_t* eye_left_ = nullptr;
    lv_obj_t* eye_right_ = nullptr;
    lv_obj_t* eye_left_glint_main_ = nullptr;
    lv_obj_t* eye_left_glint_sub_ = nullptr;
    lv_obj_t* eye_right_glint_main_ = nullptr;
    lv_obj_t* eye_right_glint_sub_ = nullptr;
    lv_obj_t* lid_top_left_ = nullptr;
    lv_obj_t* lid_top_right_ = nullptr;
    lv_obj_t* lid_bot_left_ = nullptr;
    lv_obj_t* lid_bot_right_ = nullptr;
    lv_obj_t* cheek_left_ = nullptr;
    lv_obj_t* cheek_right_ = nullptr;

    // Card de legenda compacta na base (modo vertical)
    lv_obj_t* subtitle_card_ = nullptr;
    lv_obj_t* subtitle_role_label_ = nullptr;
    lv_obj_t* subtitle_text_label_ = nullptr;

    lv_timer_t* anim_timer_ = nullptr;

    // Estado de animação (inteiro puro, cache completo para 0 redraw redundante no SPI)
    uint32_t accent_hex_ = 0xE5A353;
    VisualMood current_mood_ = VisualMood::Happy;
    VoiceState voice_state_ = VoiceState::Idle;
    uint32_t anim_tick_ = 0;
    uint32_t idle_ticks_ = 0;
    uint32_t next_blink_tick_ = 26;
    uint32_t next_gaze_tick_ = 18;
    int blink_phase_ = 0;      // 0 = aberto, >0 = fechando/piscando (suporta piscada dupla!)
    int wake_anim_ticks_ = 0;  // pulinho feliz ao despertar ("Oi Aya" / botão)
    int error_shake_ticks_ = 0;
    int gaze_x_ = 0;
    int gaze_y_ = 0;
    int target_gaze_x_ = 0;
    int target_gaze_y_ = 0;
    int last_outer_d_ = 128;
    int last_mid_d_ = 100;
    int last_core_d_ = 74;
    EyeFrameCache eye_cache_;

    RgbColorCallback rgb_cb_;
    LampModeCallback lamp_cb_;

    static void AnimTimerCb(lv_timer_t* timer);
    void TickAnimation();
    void ApplyPresenceTheme(const std::string& presence_id, bool update_leds);
    void RenderLivingEyes(bool force = false);
    void ApplyPortraitMoodGeometry(bool blinking);
    void UpdateAuraSizes(int outer_d, int mid_d, int core_d);
};

#endif  // LEWAY_K10_DISPLAY_H
