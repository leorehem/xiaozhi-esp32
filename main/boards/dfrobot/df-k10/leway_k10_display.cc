#include "leway_k10_display.h"

#include "display/lvgl_display/lvgl_theme.h"
#include <esp_log.h>
#include <cstdlib>
#include <cstring>

#define TAG "LewayK10Display"

namespace {
#if LEWAY_LANDSCAPE_EYES
constexpr uint32_t kBgObsidianHex = 0x0C0908;
constexpr uint32_t kGlintWhiteHex = 0xFFF8EE;
constexpr uint32_t kBlushWarmHex = 0x8F3C46;

int StepToward(int current, int target, int step) {
    if (current < target) {
        int next = current + step;
        return next > target ? target : next;
    }
    if (current > target) {
        int next = current - step;
        return next < target ? target : next;
    }
    return current;
}
#else
constexpr uint32_t kBgFreijoHex = 0x2B1E16;
constexpr uint32_t kCardDarkHex = 0x211610;
constexpr uint32_t kTextCreamHex = 0xF5EFE6;
constexpr uint32_t kTextMutedHex = 0xD4C6B4;
constexpr uint32_t kEyeDarkHex = 0x1D130D;

// Trunca string UTF-8 pelo final sem nunca cortar um caractere acentuado ao meio
std::string TrimSubtitleUtf8(const std::string& input, size_t max_bytes) {
    if (input.size() <= max_bytes) {
        return input;
    }
    size_t start = input.size() - max_bytes;
    while (start < input.size() && (static_cast<unsigned char>(input[start]) & 0xC0) == 0x80) {
        ++start;
    }
    size_t space_pos = input.find(' ', start);
    if (space_pos != std::string::npos && space_pos - start < 18 && space_pos + 1 < input.size()) {
        start = space_pos + 1;
    }
    return "..." + input.substr(start);
}
#endif
}  // namespace

LewayK10Display::LewayK10Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                 int width, int height, int offset_x, int offset_y,
                                 bool mirror_x, bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y,
                    swap_xy) {}

LewayK10Display::~LewayK10Display() {
    if (anim_timer_ != nullptr) {
        lv_timer_delete(anim_timer_);
        anim_timer_ = nullptr;
    }
}

void LewayK10Display::SetupUI() {
    if (setup_ui_called_) {
        return;
    }
    Display::SetupUI();
    DisplayLockGuard lock(this);
    if (!lock) {
        return;
    }

    auto* lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    const lv_font_t* text_font =
        (lvgl_theme && lvgl_theme->text_font() && lvgl_theme->text_font()->font())
            ? lvgl_theme->text_font()->font()
            : LV_FONT_DEFAULT;
    const lv_font_t* icon_font =
        (lvgl_theme && lvgl_theme->icon_font() && lvgl_theme->icon_font()->font())
            ? lvgl_theme->icon_font()->font()
            : LV_FONT_DEFAULT;

    lv_obj_t* screen = lv_screen_active();
    if (screen == nullptr) {
        return;
    }

#if LEWAY_LANDSCAPE_EYES
    // =========================================================================
    // MODO HORIZONTAL PURO (320x240) — OLHOS GRANDES, VIVOS, MEIGOS E AFETIVOS
    // Zero barra de topo, zero LeWay OS, zero Wi-Fi, zero horário, zero texto!
    // =========================================================================
    lv_obj_set_style_bg_color(screen, lv_color_hex(kBgObsidianHex), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    leway_root_ = lv_obj_create(screen);
    container_ = leway_root_;
    lv_obj_set_size(leway_root_, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(leway_root_, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(leway_root_, lv_color_hex(kBgObsidianHex), 0);
    lv_obj_set_style_bg_opa(leway_root_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(leway_root_, 0, 0);
    lv_obj_set_style_radius(leway_root_, 0, 0);
    lv_obj_set_style_pad_all(leway_root_, 0, 0);
    lv_obj_set_scrollbar_mode(leway_root_, LV_SCROLLBAR_MODE_OFF);

    // Labels da classe base mantidos ocultos e 100% transparentes (zero risco de crash)
    auto make_hidden_label = [this, icon_font]() -> lv_obj_t* {
        lv_obj_t* lbl = lv_label_create(leway_root_);
        lv_label_set_text(lbl, "");
        lv_obj_set_style_text_font(lbl, icon_font, 0);
        lv_obj_set_style_opa(lbl, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_opa(lbl, LV_OPA_TRANSP, 0);
        lv_obj_add_flag(lbl, LV_OBJ_FLAG_HIDDEN);
        return lbl;
    };
    top_bar_ = nullptr;
    status_label_ = make_hidden_label();
    network_label_ = make_hidden_label();
    mute_label_ = make_hidden_label();
    battery_label_ = make_hidden_label();
    notification_label_ = make_hidden_label();
    chat_message_label_ = make_hidden_label();

    // Bochechas rosadas sutis (aparecem em carinho, sorriso, riso e despertar!)
    auto make_cheek = [this](int x_offset) -> lv_obj_t* {
        lv_obj_t* c = lv_obj_create(leway_root_);
        lv_obj_set_size(c, 34, 15);
        lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(c, lv_color_hex(kBlushWarmHex), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(c, 0, 0);
        lv_obj_set_style_pad_all(c, 0, 0);
        lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_OFF);
        lv_obj_align(c, LV_ALIGN_CENTER, x_offset, 56);
        lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
        return c;
    };
    cheek_left_ = make_cheek(-104);
    cheek_right_ = make_cheek(104);

    // Helper para criar cada olho grande + duplo brilho especular + pálpebras superior e inferior
    auto make_living_eye = [this](int x_offset, lv_obj_t*& glint_main, lv_obj_t*& glint_sub,
                                  lv_obj_t*& lid_top, lv_obj_t*& lid_bot) -> lv_obj_t* {
        lv_obj_t* eye = lv_obj_create(leway_root_);
        lv_obj_set_size(eye, 86, 100);
        lv_obj_set_style_radius(eye, 34, 0);
        lv_obj_set_style_clip_corner(eye, true, 0);
        lv_obj_set_style_bg_color(eye, lv_color_hex(accent_hex_), 0);
        lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(eye, 0, 0);
        lv_obj_set_style_pad_all(eye, 0, 0);
        lv_obj_set_scrollbar_mode(eye, LV_SCROLLBAR_MODE_OFF);
        lv_obj_align(eye, LV_ALIGN_CENTER, x_offset, -4);

        // Brilho principal meigo (dá vida e doçura imediata aos olhos!)
        glint_main = lv_obj_create(eye);
        lv_obj_set_size(glint_main, 22, 22);
        lv_obj_set_style_radius(glint_main, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(glint_main, lv_color_hex(kGlintWhiteHex), 0);
        lv_obj_set_style_bg_opa(glint_main, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(glint_main, 0, 0);
        lv_obj_set_scrollbar_mode(glint_main, LV_SCROLLBAR_MODE_OFF);
        lv_obj_align(glint_main, LV_ALIGN_TOP_RIGHT, -16, 16);

        // Micro brilho secundário (efeito Pixar/olho vivo)
        glint_sub = lv_obj_create(eye);
        lv_obj_set_size(glint_sub, 10, 10);
        lv_obj_set_style_radius(glint_sub, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(glint_sub, lv_color_hex(kGlintWhiteHex), 0);
        lv_obj_set_style_bg_opa(glint_sub, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(glint_sub, 0, 0);
        lv_obj_set_scrollbar_mode(glint_sub, LV_SCROLLBAR_MODE_OFF);
        lv_obj_align(glint_sub, LV_ALIGN_BOTTOM_LEFT, 18, -20);

        // Pálpebra superior (cor do fundo: cria olhar sonolento, pensativo, terno ou emburrado)
        lid_top = lv_obj_create(eye);
        lv_obj_set_size(lid_top, 112, 0);
        lv_obj_set_style_radius(lid_top, 18, 0);
        lv_obj_set_style_bg_color(lid_top, lv_color_hex(kBgObsidianHex), 0);
        lv_obj_set_style_bg_opa(lid_top, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(lid_top, 0, 0);
        lv_obj_set_scrollbar_mode(lid_top, LV_SCROLLBAR_MODE_OFF);
        lv_obj_align(lid_top, LV_ALIGN_TOP_MID, 0, -10);
        lv_obj_add_flag(lid_top, LV_OBJ_FLAG_HIDDEN);

        // Pálpebra inferior arredondada (sobe de baixo formando sorriso curvo "^ ^" super meigo!)
        lid_bot = lv_obj_create(eye);
        lv_obj_set_size(lid_bot, 116, 0);
        lv_obj_set_style_radius(lid_bot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(lid_bot, lv_color_hex(kBgObsidianHex), 0);
        lv_obj_set_style_bg_opa(lid_bot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(lid_bot, 0, 0);
        lv_obj_set_scrollbar_mode(lid_bot, LV_SCROLLBAR_MODE_OFF);
        lv_obj_align(lid_bot, LV_ALIGN_BOTTOM_MID, 0, 32);
        lv_obj_add_flag(lid_bot, LV_OBJ_FLAG_HIDDEN);

        return eye;
    };

    eye_left_ = make_living_eye(-72, eye_left_glint_main_, eye_left_glint_sub_, lid_top_left_,
                                lid_bot_left_);
    eye_right_ = make_living_eye(72, eye_right_glint_main_, eye_right_glint_sub_, lid_top_right_,
                                 lid_bot_right_);

    ApplyPresenceTheme("samantha", false);
    // Começa com um despertar fofo e caloroso
    wake_anim_ticks_ = 8;
    RenderLivingEyes(true);

    // Timer suave (~8 FPS = 120ms): só redesenha no SPI quando há mudança real (zero risco Watchdog)
    anim_timer_ = lv_timer_create(AnimTimerCb, 120, this);
#else
    // =========================================================================
    // MODO VERTICAL CLÁSSICO (240x320) — LeWay OS + Nome + Aura + Card Compacto
    // =========================================================================
    lv_obj_set_style_bg_color(screen, lv_color_hex(kBgFreijoHex), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(kTextCreamHex), 0);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    leway_root_ = lv_obj_create(screen);
    container_ = leway_root_;
    lv_obj_set_size(leway_root_, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(leway_root_, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(leway_root_, lv_color_hex(kBgFreijoHex), 0);
    lv_obj_set_style_bg_opa(leway_root_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(leway_root_, 0, 0);
    lv_obj_set_style_radius(leway_root_, 0, 0);
    lv_obj_set_style_pad_all(leway_root_, 0, 0);
    lv_obj_set_scrollbar_mode(leway_root_, LV_SCROLLBAR_MODE_OFF);

    top_bar_ = lv_obj_create(leway_root_);
    lv_obj_set_size(top_bar_, LV_HOR_RES, 26);
    lv_obj_align(top_bar_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_bar_, lv_color_hex(kCardDarkHex), 0);
    lv_obj_set_style_bg_opa(top_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(top_bar_, 0, 0);
    lv_obj_set_style_radius(top_bar_, 0, 0);
    lv_obj_set_style_pad_hor(top_bar_, 10, 0);
    lv_obj_set_style_pad_ver(top_bar_, 0, 0);
    lv_obj_set_scrollbar_mode(top_bar_, LV_SCROLLBAR_MODE_OFF);

    header_brand_label_ = lv_label_create(top_bar_);
    lv_label_set_text(header_brand_label_, "LeWay OS");
    lv_obj_set_style_text_color(header_brand_label_, lv_color_hex(accent_hex_), 0);
    lv_obj_set_style_text_letter_space(header_brand_label_, 0, 0);
    lv_obj_align(header_brand_label_, LV_ALIGN_LEFT_MID, 0, 0);

    status_label_ = lv_label_create(top_bar_);
    lv_label_set_text(status_label_, "Em espera");
    lv_obj_set_style_text_color(status_label_, lv_color_hex(kTextMutedHex), 0);
    lv_obj_align(status_label_, LV_ALIGN_CENTER, 16, 0);

    network_label_ = lv_label_create(top_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);
    lv_obj_set_style_text_color(network_label_, lv_color_hex(kTextMutedHex), 0);
    lv_obj_align(network_label_, LV_ALIGN_RIGHT_MID, 0, 0);

    notification_label_ = lv_label_create(leway_root_);
    lv_label_set_text(notification_label_, "");
    lv_obj_set_width(notification_label_, LV_HOR_RES - 24);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(notification_label_, lv_color_hex(kCardDarkHex), 0);
    lv_obj_set_style_bg_opa(notification_label_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(notification_label_, lv_color_hex(kTextCreamHex), 0);
    lv_obj_set_style_pad_hor(notification_label_, 8, 0);
    lv_obj_set_style_pad_ver(notification_label_, 4, 0);
    lv_obj_set_style_radius(notification_label_, 8, 0);
    lv_obj_align(notification_label_, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    mute_label_ = lv_label_create(top_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);
    lv_obj_add_flag(mute_label_, LV_OBJ_FLAG_HIDDEN);

    battery_label_ = lv_label_create(top_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);
    lv_obj_add_flag(battery_label_, LV_OBJ_FLAG_HIDDEN);

    presence_name_label_ = lv_label_create(leway_root_);
    lv_label_set_text(presence_name_label_, "Samantha");
    lv_obj_set_style_text_color(presence_name_label_, lv_color_hex(kTextCreamHex), 0);
    lv_obj_align(presence_name_label_, LV_ALIGN_TOP_MID, 0, 38);

    aura_stage_ = lv_obj_create(leway_root_);
    lv_obj_set_size(aura_stage_, 152, 152);
    lv_obj_align(aura_stage_, LV_ALIGN_TOP_MID, 0, 74);
    lv_obj_set_style_bg_color(aura_stage_, lv_color_hex(kBgFreijoHex), 0);
    lv_obj_set_style_bg_opa(aura_stage_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(aura_stage_, 0, 0);
    lv_obj_set_style_pad_all(aura_stage_, 0, 0);
    lv_obj_set_scrollbar_mode(aura_stage_, LV_SCROLLBAR_MODE_OFF);

    aura_outer_ = lv_obj_create(aura_stage_);
    lv_obj_set_size(aura_outer_, last_outer_d_, last_outer_d_);
    lv_obj_center(aura_outer_);
    lv_obj_set_style_radius(aura_outer_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(aura_outer_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(aura_outer_, 0, 0);
    lv_obj_set_scrollbar_mode(aura_outer_, LV_SCROLLBAR_MODE_OFF);

    aura_mid_ = lv_obj_create(aura_stage_);
    lv_obj_set_size(aura_mid_, last_mid_d_, last_mid_d_);
    lv_obj_center(aura_mid_);
    lv_obj_set_style_radius(aura_mid_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(aura_mid_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(aura_mid_, 0, 0);
    lv_obj_set_scrollbar_mode(aura_mid_, LV_SCROLLBAR_MODE_OFF);

    aura_core_ = lv_obj_create(aura_stage_);
    lv_obj_set_size(aura_core_, last_core_d_, last_core_d_);
    lv_obj_center(aura_core_);
    lv_obj_set_style_radius(aura_core_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(aura_core_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(aura_core_, 2, 0);
    lv_obj_set_style_border_color(aura_core_, lv_color_hex(kTextCreamHex), 0);
    lv_obj_set_style_border_opa(aura_core_, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(aura_core_, LV_SCROLLBAR_MODE_OFF);

    eye_left_ = lv_obj_create(aura_core_);
    lv_obj_set_size(eye_left_, 12, 16);
    lv_obj_set_style_radius(eye_left_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(eye_left_, lv_color_hex(kEyeDarkHex), 0);
    lv_obj_set_style_bg_opa(eye_left_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eye_left_, 0, 0);
    lv_obj_align(eye_left_, LV_ALIGN_CENTER, -13, -2);

    eye_right_ = lv_obj_create(aura_core_);
    lv_obj_set_size(eye_right_, 12, 16);
    lv_obj_set_style_radius(eye_right_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(eye_right_, lv_color_hex(kEyeDarkHex), 0);
    lv_obj_set_style_bg_opa(eye_right_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eye_right_, 0, 0);
    lv_obj_align(eye_right_, LV_ALIGN_CENTER, 13, -2);

    subtitle_card_ = lv_obj_create(leway_root_);
    lv_obj_set_size(subtitle_card_, LV_HOR_RES - 20, 62);
    lv_obj_align(subtitle_card_, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(subtitle_card_, lv_color_hex(kCardDarkHex), 0);
    lv_obj_set_style_bg_opa(subtitle_card_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(subtitle_card_, 11, 0);
    lv_obj_set_style_border_width(subtitle_card_, 1, 0);
    lv_obj_set_style_border_color(subtitle_card_, lv_color_hex(accent_hex_), 0);
    lv_obj_set_style_border_opa(subtitle_card_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(subtitle_card_, 10, 0);
    lv_obj_set_style_pad_ver(subtitle_card_, 5, 0);
    lv_obj_set_scrollbar_mode(subtitle_card_, LV_SCROLLBAR_MODE_OFF);

    subtitle_role_label_ = lv_label_create(subtitle_card_);
    lv_label_set_text(subtitle_role_label_, "EM ESPERA");
    lv_obj_set_style_text_color(subtitle_role_label_, lv_color_hex(kTextMutedHex), 0);
    lv_obj_align(subtitle_role_label_, LV_ALIGN_TOP_LEFT, 0, 0);

    subtitle_text_label_ = lv_label_create(subtitle_card_);
    chat_message_label_ = subtitle_text_label_;
    lv_obj_set_width(subtitle_text_label_, LV_HOR_RES - 42);
    lv_label_set_long_mode(subtitle_text_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(subtitle_text_label_, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(subtitle_text_label_, lv_color_hex(kTextCreamHex), 0);
    lv_label_set_text(subtitle_text_label_, "Diga Oi Aya ou aperte A.");
    lv_obj_align(subtitle_text_label_, LV_ALIGN_TOP_LEFT, 0, 17);

    ApplyPresenceTheme("samantha", false);
    anim_timer_ = lv_timer_create(AnimTimerCb, 250, this);
#endif
}

void LewayK10Display::SetStatus(const char* status) {
    if (!setup_ui_called_ || status == nullptr) return;
    DisplayLockGuard lock(this);
    if (!lock) return;

    VoiceState prev_state = voice_state_;
    std::string st(status);

    bool is_error = (st.find("Error") != std::string::npos ||
                     st.find("Fail") != std::string::npos ||
                     st.find("Offline") != std::string::npos ||
                     st.find("Disconnect") != std::string::npos ||
                     st.find("Erro") != std::string::npos ||
                     st.find("错误") != std::string::npos ||
                     st.find("失败") != std::string::npos);

    if (st.find("Speaking") != std::string::npos || st.find("说话") != std::string::npos ||
        st.find("Falando") != std::string::npos) {
        voice_state_ = VoiceState::Speaking;
        idle_ticks_ = 0;
        if (current_mood_ == VisualMood::Sleepy || current_mood_ == VisualMood::Grumpy) {
            current_mood_ = VisualMood::Happy;
        }
    } else if (st.find("Listening") != std::string::npos || st.find("聆听") != std::string::npos ||
               st.find("Ouvindo") != std::string::npos) {
        voice_state_ = VoiceState::Listening;
        idle_ticks_ = 0;
        // Acordou de espera -> pulinho alegre + olhar curioso e brilhante!
        if (prev_state == VoiceState::Idle) {
            wake_anim_ticks_ = 10;
            current_mood_ = VisualMood::Happy;
            target_gaze_x_ = 0;
            target_gaze_y_ = -4;
        }
    } else {
        voice_state_ = VoiceState::Idle;
        if (is_error) {
            current_mood_ = VisualMood::Grumpy;
            error_shake_ticks_ = 12;
        } else if (prev_state != VoiceState::Idle && current_mood_ == VisualMood::Grumpy) {
            current_mood_ = VisualMood::Neutral;
        }
    }

#if LEWAY_LANDSCAPE_EYES
    RenderLivingEyes(false);
#else
    if (notification_label_ != nullptr) {
        lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (status_label_ != nullptr) {
        lv_obj_remove_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
    }

    if (voice_state_ == VoiceState::Speaking) {
        if (status_label_) lv_label_set_text(status_label_, "Falando");
        if (subtitle_role_label_) {
            lv_label_set_text(subtitle_role_label_, "FALANDO");
            lv_obj_set_style_text_color(subtitle_role_label_, lv_color_hex(accent_hex_), 0);
        }
    } else if (voice_state_ == VoiceState::Listening) {
        if (status_label_) lv_label_set_text(status_label_, "Ouvindo...");
        if (subtitle_role_label_) {
            lv_label_set_text(subtitle_role_label_, "OUVINDO");
            lv_obj_set_style_text_color(subtitle_role_label_, lv_color_hex(accent_hex_), 0);
        }
    } else if (st.find("Connecting") != std::string::npos || st.find("连接") != std::string::npos) {
        if (status_label_) lv_label_set_text(status_label_, "Conectando...");
        if (subtitle_role_label_) {
            lv_label_set_text(subtitle_role_label_, "CONECTANDO");
            lv_obj_set_style_text_color(subtitle_role_label_, lv_color_hex(kTextMutedHex), 0);
        }
    } else {
        if (status_label_) lv_label_set_text(status_label_, "Em espera");
        if (subtitle_role_label_) {
            lv_label_set_text(subtitle_role_label_, "EM ESPERA");
            lv_obj_set_style_text_color(subtitle_role_label_, lv_color_hex(kTextMutedHex), 0);
        }
        if (subtitle_text_label_) {
            lv_label_set_text(subtitle_text_label_, "Diga Oi Aya ou aperte A.");
        }
    }
#endif
    last_status_update_time_ = std::chrono::system_clock::now();
}

bool LewayK10Display::SetEmotion(const char* emotion) {
    if (!setup_ui_called_ || emotion == nullptr) return false;
    DisplayLockGuard lock(this);
    if (!lock) return false;

    std::string emo(emotion);

    // Protocolo LeWay: "presence:<id>" troca paleta/cor dos olhos e LEDs RGB
    constexpr const char* kPresencePrefix = "presence:";
    if (emo.rfind(kPresencePrefix, 0) == 0) {
        ApplyPresenceTheme(emo.substr(std::strlen(kPresencePrefix)), true);
        wake_anim_ticks_ = 8;
        return true;
    }

    // Protocolo LeWay Abajur: "lamp:RRGGBB" trava cor fixa | "lamp:off" volta ao automático
    constexpr const char* kLampPrefix = "lamp:";
    if (emo.rfind(kLampPrefix, 0) == 0) {
        std::string val = emo.substr(std::strlen(kLampPrefix));
        if (val == "off" || val == "auto") {
            if (lamp_cb_) {
                lamp_cb_(false, 0, 0, 0);
            }
            return true;
        }
        if (val.size() == 6) {
            char* endptr = nullptr;
            unsigned long rgb = std::strtoul(val.c_str(), &endptr, 16);
            if (endptr != nullptr && *endptr == '\0') {
                uint8_t r = static_cast<uint8_t>((rgb >> 16) & 0xFF);
                uint8_t g = static_cast<uint8_t>((rgb >> 8) & 0xFF);
                uint8_t b = static_cast<uint8_t>(rgb & 0xFF);
                if (lamp_cb_) {
                    lamp_cb_(true, r, g, b);
                } else if (rgb_cb_) {
                    rgb_cb_(r, g, b);
                }
                return true;
            }
        }
        return true;
    }

    idle_ticks_ = 0;

    if (emo == "laughing" || emo == "funny" || emo == "silly") {
        current_mood_ = VisualMood::Laughing;
    } else if (emo == "happy" || emo == "cool" || emo == "confident" || emo == "winking") {
        current_mood_ = VisualMood::Happy;
    } else if (emo == "thinking" || emo == "confused") {
        current_mood_ = VisualMood::Thinking;
        target_gaze_x_ = 14;
        target_gaze_y_ = -8;
    } else if (emo == "surprised" || emo == "shocked") {
        current_mood_ = VisualMood::Surprised;
        target_gaze_x_ = 0;
        target_gaze_y_ = -4;
    } else if (emo == "sad" || emo == "crying") {
        current_mood_ = VisualMood::Sad;
        target_gaze_x_ = 0;
        target_gaze_y_ = 10;
    } else if (emo == "loving" || emo == "kissy") {
        current_mood_ = VisualMood::Loving;
        target_gaze_x_ = 0;
        target_gaze_y_ = -2;
    } else if (emo == "sleepy" || emo == "relaxed") {
        current_mood_ = VisualMood::Sleepy;
    } else if (emo == "angry" || emo == "grumpy" || emo == "error") {
        current_mood_ = VisualMood::Grumpy;
        error_shake_ticks_ = 10;
    } else {
        current_mood_ = VisualMood::Neutral;
    }

#if LEWAY_LANDSCAPE_EYES
    RenderLivingEyes(false);
#else
    ApplyPortraitMoodGeometry(blink_phase_ > 0);
#endif
    return true;
}

void LewayK10Display::SetChatMessage(const char* role, const char* content) {
#if LEWAY_LANDSCAPE_EYES
    (void)role;
    (void)content;
    return;
#else
    if (!setup_ui_called_ || content == nullptr) return;
    DisplayLockGuard lock(this);
    if (!lock) return;

    std::string text(content);
    bool is_user = (role != nullptr && std::strcmp(role, "user") == 0);

    if (subtitle_role_label_ != nullptr) {
        if (is_user) {
            lv_label_set_text(subtitle_role_label_, "VOCE");
            lv_obj_set_style_text_color(subtitle_role_label_, lv_color_hex(kTextMutedHex), 0);
        } else if (voice_state_ == VoiceState::Speaking) {
            lv_label_set_text(subtitle_role_label_, "FALANDO");
            lv_obj_set_style_text_color(subtitle_role_label_, lv_color_hex(accent_hex_), 0);
        } else if (voice_state_ == VoiceState::Listening) {
            lv_label_set_text(subtitle_role_label_, "OUVINDO");
            lv_obj_set_style_text_color(subtitle_role_label_, lv_color_hex(accent_hex_), 0);
        } else {
            lv_label_set_text(subtitle_role_label_, "EM ESPERA");
            lv_obj_set_style_text_color(subtitle_role_label_, lv_color_hex(kTextMutedHex), 0);
        }
    }

    if (subtitle_text_label_ != nullptr) {
        if (text.empty()) {
            lv_label_set_text(subtitle_text_label_, "Diga Oi Aya ou aperte A.");
            return;
        }
        std::string trimmed = TrimSubtitleUtf8(text, 72);
        lv_label_set_text(subtitle_text_label_, trimmed.c_str());
    }
#endif
}

void LewayK10Display::RemoveChatMessage(const char* /*role*/, const char* /*content*/) {}

void LewayK10Display::ClearChatMessages() {
#if LEWAY_LANDSCAPE_EYES
    return;
#else
    if (!setup_ui_called_) return;
    DisplayLockGuard lock(this);
    if (!lock) return;
    if (subtitle_role_label_ != nullptr) {
        lv_label_set_text(subtitle_role_label_, "EM ESPERA");
        lv_obj_set_style_text_color(subtitle_role_label_, lv_color_hex(kTextMutedHex), 0);
    }
    if (subtitle_text_label_ != nullptr) {
        lv_label_set_text(subtitle_text_label_, "Diga Oi Aya ou aperte A.");
    }
#endif
}

void LewayK10Display::SetPreviewImage(std::unique_ptr<LvglImage> /*image*/) {}

void LewayK10Display::SetHideSubtitle(bool hide) {
    hide_subtitle_ = hide;
#if LEWAY_LANDSCAPE_EYES
    return;
#else
    if (!setup_ui_called_) return;
    DisplayLockGuard lock(this);
    if (!lock) return;
    if (subtitle_card_ != nullptr) {
        if (hide) {
            lv_obj_add_flag(subtitle_card_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(subtitle_card_, LV_OBJ_FLAG_HIDDEN);
        }
    }
#endif
}

void LewayK10Display::SetTheme(Theme* theme) {
    if (theme == nullptr) return;
    DisplayLockGuard lock(this);
    if (!lock) return;

    current_theme_ = theme;
    auto* lvgl_theme = static_cast<LvglTheme*>(theme);

    const lv_font_t* text_font =
        (lvgl_theme->text_font() && lvgl_theme->text_font()->font())
            ? lvgl_theme->text_font()->font()
            : nullptr;
    const lv_font_t* icon_font =
        (lvgl_theme->icon_font() && lvgl_theme->icon_font()->font())
            ? lvgl_theme->icon_font()->font()
            : nullptr;

    lv_obj_t* screen = lv_screen_active();
    if (text_font != nullptr && screen != nullptr) {
        lv_obj_set_style_text_font(screen, text_font, 0);
    }
    if (icon_font != nullptr) {
        if (network_label_) lv_obj_set_style_text_font(network_label_, icon_font, 0);
        if (mute_label_) lv_obj_set_style_text_font(mute_label_, icon_font, 0);
        if (battery_label_) lv_obj_set_style_text_font(battery_label_, icon_font, 0);
    }
}

void LewayK10Display::ApplyPresenceTheme(const std::string& presence_id, bool update_leds) {
    static const PresenceTheme kThemes[] = {
        {"dora", "Dora", 0x3D3929, 0x5A6144, 0x99B87E, 30, 50, 22},
        {"ze_baiano", "Ze Baiano", 0x4E341E, 0x875628, 0xF0A64A, 65, 38, 8},
        {"jorge", "Jorge Carioca", 0x263832, 0x295E56, 0x38C2B4, 10, 52, 48},
        {"papai_noel", "Papai Noel", 0x492B20, 0x7E4230, 0xE2795A, 68, 22, 14},
        {"saci", "Saci", 0x4D2A1C, 0x8B3E24, 0xEE6D44, 70, 20, 8},
        {"emilia", "Emilia", 0x4C2B31, 0x884457, 0xEE7AA0, 65, 18, 36},
        {"lyra", "Lyra", 0x4A2B30, 0x82444F, 0xDC7A8E, 68, 22, 34},
        {"samantha", "Samantha", 0x4F3821, 0x8D5F2C, 0xF2B15C, 72, 46, 12},
        {"lua", "Lua", 0x26214A, 0x5B4B9E, 0xB8A6FF, 44, 34, 78},
        {"bia_banca", "Bia da Banca", 0x1E354A, 0x2F668A, 0x5CB3E0, 22, 54, 72},
        {"bento_cangaco", "Seu Bento", 0x452A1B, 0x7A4728, 0xD48248, 66, 36, 14},
    };

    const PresenceTheme* selected = &kThemes[7];  // Samantha warm amber default
    for (const auto& t : kThemes) {
        if (presence_id == t.id) {
            selected = &t;
            break;
        }
    }

    accent_hex_ = selected->core_hex;
    lv_color_t core_c = lv_color_hex(selected->core_hex);

#if LEWAY_LANDSCAPE_EYES
    if (eye_left_) lv_obj_set_style_bg_color(eye_left_, core_c, 0);
    if (eye_right_) lv_obj_set_style_bg_color(eye_right_, core_c, 0);
#else
    lv_color_t outer_c = lv_color_hex(selected->outer_hex);
    lv_color_t mid_c = lv_color_hex(selected->mid_hex);
    if (header_brand_label_) lv_obj_set_style_text_color(header_brand_label_, core_c, 0);
    if (presence_name_label_) lv_label_set_text(presence_name_label_, selected->display_name);
    if (aura_outer_) lv_obj_set_style_bg_color(aura_outer_, outer_c, 0);
    if (aura_mid_) lv_obj_set_style_bg_color(aura_mid_, mid_c, 0);
    if (aura_core_) lv_obj_set_style_bg_color(aura_core_, core_c, 0);
    if (subtitle_card_) lv_obj_set_style_border_color(subtitle_card_, core_c, 0);
    if (subtitle_role_label_ && voice_state_ != VoiceState::Idle) {
        lv_obj_set_style_text_color(subtitle_role_label_, core_c, 0);
    }
#endif

    if (update_leds && rgb_cb_) {
        rgb_cb_(selected->led_r, selected->led_g, selected->led_b);
    }
    ESP_LOGI(TAG, "Tema LeWay aplicado: %s (#%06lX)", selected->display_name,
             static_cast<unsigned long>(accent_hex_));
}

void LewayK10Display::RenderLivingEyes(bool force) {
#if LEWAY_LANDSCAPE_EYES
    if (!eye_left_ || !eye_right_) return;

    // Sono progressivo em silêncio (120ms/tick: ~35s -> sonolento, ~90s -> dormindo fofo)
    VisualMood effective_mood = current_mood_;
    if (voice_state_ == VoiceState::Idle && wake_anim_ticks_ == 0 && error_shake_ticks_ == 0 &&
        current_mood_ != VisualMood::Grumpy) {
        if (idle_ticks_ > 750) {
            effective_mood = VisualMood::Sleepy;
        } else if (idle_ticks_ > 300) {
            effective_mood = VisualMood::Neutral;
        }
    }

    // Deslocamentos orgânicos (pulinho de despertar, chacoalho de erro, pulso de fala)
    int bounce_y = 0;
    if (wake_anim_ticks_ > 0) {
        static constexpr int kWakeBounce[10] = {0, -4, -10, -14, -10, -4, -8, -4, 0, 0};
        int idx = 10 - wake_anim_ticks_;
        if (idx >= 0 && idx < 10) bounce_y = kWakeBounce[idx];
    }

    int shake_x = 0;
    if (error_shake_ticks_ > 0) {
        static constexpr int kShake[12] = {-10, 10, -8, 8, -6, 6, -4, 4, -2, 2, 0, 0};
        int idx = 12 - error_shake_ticks_;
        if (idx >= 0 && idx < 12) shake_x = kShake[idx];
    }

    int speak_h = 0;
    int speak_y = 0;
    if (voice_state_ == VoiceState::Speaking) {
        static constexpr int kSpeakWave[8] = {0, 6, 12, 8, 2, 10, 6, 0};
        speak_h = kSpeakWave[anim_tick_ % 8];
        speak_y = -(speak_h / 3);
    } else if (voice_state_ == VoiceState::Listening) {
        static constexpr int kListenWave[6] = {0, 3, 6, 4, 2, 0};
        speak_h = kListenWave[(anim_tick_ >> 1) % 6];
    }

    bool blinking_now = (blink_phase_ == 1 || blink_phase_ == 3);

    int lw = 86, lh = 100, lrad = 34;
    int rw = 86, rh = 100, rrad = 34;
    int l_top_h = 0, r_top_h = 0;
    int l_bot_h = 0, r_bot_h = 0;
    int mood_y = -4;
    bool glint_vis = true;
    int glint_big = 22;
    bool cheeks_vis = false;

    if (blinking_now) {
        // Piscada macia e expressiva (arco fechado)
        lw = rw = 94;
        lh = rh = 10;
        lrad = rrad = 5;
        mood_y = 4;
        glint_vis = false;
        cheeks_vis = (effective_mood == VisualMood::Loving ||
                      effective_mood == VisualMood::Laughing || wake_anim_ticks_ > 0);
    } else if (wake_anim_ticks_ > 2) {
        // Despertar radiante com olhos sorridentes e bochechas rosadas!
        lw = rw = 90;
        lh = rh = 96;
        lrad = rrad = 38;
        l_bot_h = r_bot_h = 56;
        mood_y = -8;
        glint_big = 24;
        cheeks_vis = true;
    } else {
        switch (effective_mood) {
            case VisualMood::Loving:
                // Olhos super meigos sorrindo em arco + bochechas rosadas + brilho grande!
                lw = rw = 92;
                lh = rh = 96 + (speak_h / 2);
                lrad = rrad = 40;
                l_bot_h = r_bot_h = 58;
                mood_y = -6;
                glint_big = 26;
                cheeks_vis = true;
                break;
            case VisualMood::Laughing:
                // Sorriso aberto contagiante ("^ ^")
                lw = rw = 94;
                lh = rh = 92 + (speak_h / 2);
                lrad = rrad = 42;
                l_bot_h = r_bot_h = 66;
                mood_y = -10 + ((anim_tick_ & 1U) ? -3 : 1);
                glint_vis = false;
                cheeks_vis = true;
                break;
            case VisualMood::Happy:
                // Meigo, luminoso e acolhedor (leve curva de sorriso embaixo)
                lw = rw = 88;
                lh = rh = 98 + speak_h;
                lrad = rrad = 36;
                l_bot_h = r_bot_h = (voice_state_ == VoiceState::Speaking) ? 46 : 36;
                mood_y = -6;
                glint_big = 24;
                cheeks_vis = (voice_state_ == VoiceState::Speaking);
                break;
            case VisualMood::Surprised:
                // Encantado / arregalado com brilho estelar
                lw = rw = 92;
                lh = rh = 118 + (speak_h / 2);
                lrad = rrad = 42;
                mood_y = -8;
                glint_big = 28;
                break;
            case VisualMood::Thinking:
                // Curioso assimétrico (um olho bem aberto, outro semicerrado olhando pro lado)
                lw = 84; lh = 100 + (speak_h / 2); lrad = 34; l_top_h = 0;
                rw = 84; rh = 92;                  rrad = 34; r_top_h = 36;
                mood_y = -6;
                glint_big = 20;
                break;
            case VisualMood::Sad:
                // Olhar terno e empático (pálpebra suave + levemente abaixado)
                lw = rw = 82;
                lh = rh = 86;
                lrad = rrad = 32;
                l_top_h = r_top_h = 28;
                mood_y = 10;
                glint_big = 18;
                break;
            case VisualMood::Grumpy:
                // Emburradinho fofo quando cai Wi-Fi ou dá erro
                lw = rw = 88;
                lh = rh = 82;
                lrad = rrad = 26;
                l_top_h = r_top_h = 38;
                mood_y = 4;
                glint_big = 16;
                break;
            case VisualMood::Sleepy:
                // Cochilando tranquilo (respiração bem lenta)
                lw = rw = 90;
                lh = rh = (idle_ticks_ > 900) ? 26 : 54;
                lrad = rrad = (idle_ticks_ > 900) ? 13 : 26;
                l_top_h = r_top_h = (idle_ticks_ > 900) ? 0 : 28;
                mood_y = 10 + (((anim_tick_ >> 3) & 1U) ? 2 : 0);
                glint_vis = (idle_ticks_ <= 900);
                glint_big = 14;
                break;
            case VisualMood::Neutral:
            default:
                // Calmo e presente (leve sonolência se parado > 35s)
                lw = rw = 86;
                lh = rh = 100 + speak_h;
                lrad = rrad = 34;
                l_top_h = r_top_h = (voice_state_ == VoiceState::Idle && idle_ticks_ > 300) ? 22 : 0;
                mood_y = -4;
                glint_big = 22;
                break;
        }
    }

    int lx = -72 + gaze_x_ + shake_x;
    int rx = 72 + gaze_x_ + shake_x;
    int ly = mood_y + gaze_y_ + bounce_y + speak_y;
    int ry = mood_y + gaze_y_ + bounce_y + speak_y;

    // Aplica no LVGL apenas o que mudou (zero sobrecarga no barramento SPI!)
    if (force || lw != eye_cache_.lw || lh != eye_cache_.lh) {
        lv_obj_set_size(eye_left_, lw, lh);
        eye_cache_.lw = lw;
        eye_cache_.lh = lh;
    }
    if (force || rw != eye_cache_.rw || rh != eye_cache_.rh) {
        lv_obj_set_size(eye_right_, rw, rh);
        eye_cache_.rw = rw;
        eye_cache_.rh = rh;
    }
    if (force || lrad != eye_cache_.lrad) {
        lv_obj_set_style_radius(eye_left_, lrad, 0);
        eye_cache_.lrad = lrad;
    }
    if (force || rrad != eye_cache_.rrad) {
        lv_obj_set_style_radius(eye_right_, rrad, 0);
        eye_cache_.rrad = rrad;
    }
    if (force || lx != eye_cache_.lx || ly != eye_cache_.ly) {
        lv_obj_align(eye_left_, LV_ALIGN_CENTER, lx, ly);
        eye_cache_.lx = lx;
        eye_cache_.ly = ly;
    }
    if (force || rx != eye_cache_.rx || ry != eye_cache_.ry) {
        lv_obj_align(eye_right_, LV_ALIGN_CENTER, rx, ry);
        eye_cache_.rx = rx;
        eye_cache_.ry = ry;
    }

    // Pálpebras superiores
    auto apply_top_lid = [](lv_obj_t* lid, int h, int& cached_h, bool f) {
        if (!lid || (!f && h == cached_h)) return;
        cached_h = h;
        if (h <= 0) {
            lv_obj_add_flag(lid, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_set_size(lid, 114, h + 10);
            lv_obj_align(lid, LV_ALIGN_TOP_MID, 0, -10);
            lv_obj_remove_flag(lid, LV_OBJ_FLAG_HIDDEN);
        }
    };
    apply_top_lid(lid_top_left_, l_top_h, eye_cache_.l_top_h, force);
    apply_top_lid(lid_top_right_, r_top_h, eye_cache_.r_top_h, force);

    // Pálpebras inferiores curvas (sorriso meigo "^ ^")
    auto apply_bot_lid = [](lv_obj_t* lid, int h, int& cached_h, bool f) {
        if (!lid || (!f && h == cached_h)) return;
        cached_h = h;
        if (h <= 0) {
            lv_obj_add_flag(lid, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_set_size(lid, 120, h + 32);
            lv_obj_align(lid, LV_ALIGN_BOTTOM_MID, 0, 34);
            lv_obj_remove_flag(lid, LV_OBJ_FLAG_HIDDEN);
        }
    };
    apply_bot_lid(lid_bot_left_, l_bot_h, eye_cache_.l_bot_h, force);
    apply_bot_lid(lid_bot_right_, r_bot_h, eye_cache_.r_bot_h, force);

    // Brilhos especulares com paralaxe de olhar
    if (force || glint_vis != eye_cache_.glint_vis) {
        eye_cache_.glint_vis = glint_vis;
        lv_obj_t* glints[4] = {eye_left_glint_main_, eye_left_glint_sub_,
                               eye_right_glint_main_, eye_right_glint_sub_};
        for (lv_obj_t* g : glints) {
            if (!g) continue;
            if (glint_vis) {
                lv_obj_remove_flag(g, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(g, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    if (glint_vis && (force || glint_big != eye_cache_.glint_big)) {
        eye_cache_.glint_big = glint_big;
        if (eye_left_glint_main_) lv_obj_set_size(eye_left_glint_main_, glint_big, glint_big);
        if (eye_right_glint_main_) lv_obj_set_size(eye_right_glint_main_, glint_big, glint_big);
    }
    if (glint_vis && (force || gaze_x_ != eye_cache_.gaze_x || gaze_y_ != eye_cache_.gaze_y)) {
        eye_cache_.gaze_x = gaze_x_;
        eye_cache_.gaze_y = gaze_y_;
        int gx = gaze_x_ / 3;
        int gy = gaze_y_ / 3;
        if (eye_left_glint_main_) lv_obj_align(eye_left_glint_main_, LV_ALIGN_TOP_RIGHT, -16 + gx, 16 + gy);
        if (eye_right_glint_main_) lv_obj_align(eye_right_glint_main_, LV_ALIGN_TOP_RIGHT, -16 + gx, 16 + gy);
        if (eye_left_glint_sub_) lv_obj_align(eye_left_glint_sub_, LV_ALIGN_BOTTOM_LEFT, 18 + gx, -20 + gy);
        if (eye_right_glint_sub_) lv_obj_align(eye_right_glint_sub_, LV_ALIGN_BOTTOM_LEFT, 18 + gx, -20 + gy);
    }

    // Bochechas rosadas de afeto
    if (force || cheeks_vis != eye_cache_.cheeks_vis) {
        eye_cache_.cheeks_vis = cheeks_vis;
        if (cheek_left_) {
            if (cheeks_vis) lv_obj_remove_flag(cheek_left_, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(cheek_left_, LV_OBJ_FLAG_HIDDEN);
        }
        if (cheek_right_) {
            if (cheeks_vis) lv_obj_remove_flag(cheek_right_, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(cheek_right_, LV_OBJ_FLAG_HIDDEN);
        }
    }
#else
    (void)force;
#endif
}

void LewayK10Display::ApplyPortraitMoodGeometry(bool blinking) {
    if (!eye_left_ || !eye_right_) return;

    if (blinking) {
        lv_obj_set_size(eye_left_, 14, 3);
        lv_obj_set_size(eye_right_, 14, 3);
        lv_obj_align(eye_left_, LV_ALIGN_CENTER, -13, 0);
        lv_obj_align(eye_right_, LV_ALIGN_CENTER, 13, 0);
        return;
    }

    switch (current_mood_) {
        case VisualMood::Laughing:
        case VisualMood::Happy:
            lv_obj_set_size(eye_left_, 14, 13);
            lv_obj_set_size(eye_right_, 14, 13);
            lv_obj_align(eye_left_, LV_ALIGN_CENTER, -13, -3);
            lv_obj_align(eye_right_, LV_ALIGN_CENTER, 13, -3);
            break;
        case VisualMood::Surprised:
            lv_obj_set_size(eye_left_, 14, 20);
            lv_obj_set_size(eye_right_, 14, 20);
            lv_obj_align(eye_left_, LV_ALIGN_CENTER, -13, -2);
            lv_obj_align(eye_right_, LV_ALIGN_CENTER, 13, -2);
            break;
        case VisualMood::Thinking:
            lv_obj_set_size(eye_left_, 12, 15);
            lv_obj_set_size(eye_right_, 12, 10);
            lv_obj_align(eye_left_, LV_ALIGN_CENTER, -10, -4);
            lv_obj_align(eye_right_, LV_ALIGN_CENTER, 16, -3);
            break;
        case VisualMood::Sad:
            lv_obj_set_size(eye_left_, 12, 9);
            lv_obj_set_size(eye_right_, 12, 9);
            lv_obj_align(eye_left_, LV_ALIGN_CENTER, -13, 4);
            lv_obj_align(eye_right_, LV_ALIGN_CENTER, 13, 4);
            break;
        case VisualMood::Sleepy:
        case VisualMood::Grumpy:
            lv_obj_set_size(eye_left_, 14, 5);
            lv_obj_set_size(eye_right_, 14, 5);
            lv_obj_align(eye_left_, LV_ALIGN_CENTER, -13, 2);
            lv_obj_align(eye_right_, LV_ALIGN_CENTER, 13, 2);
            break;
        case VisualMood::Loving:
        case VisualMood::Neutral:
        default:
            lv_obj_set_size(eye_left_, 12, 16);
            lv_obj_set_size(eye_right_, 12, 16);
            lv_obj_align(eye_left_, LV_ALIGN_CENTER, -13, -2);
            lv_obj_align(eye_right_, LV_ALIGN_CENTER, 13, -2);
            break;
    }
}

void LewayK10Display::UpdateAuraSizes(int outer_d, int mid_d, int core_d) {
    if (aura_outer_ && outer_d != last_outer_d_) {
        last_outer_d_ = outer_d;
        lv_obj_set_size(aura_outer_, outer_d, outer_d);
        lv_obj_center(aura_outer_);
    }
    if (aura_mid_ && mid_d != last_mid_d_) {
        last_mid_d_ = mid_d;
        lv_obj_set_size(aura_mid_, mid_d, mid_d);
        lv_obj_center(aura_mid_);
    }
    if (aura_core_ && core_d != last_core_d_) {
        last_core_d_ = core_d;
        lv_obj_set_size(aura_core_, core_d, core_d);
        lv_obj_center(aura_core_);
    }
}

void LewayK10Display::AnimTimerCb(lv_timer_t* timer) {
    auto* self = static_cast<LewayK10Display*>(lv_timer_get_user_data(timer));
    if (self != nullptr) {
        self->TickAnimation();
    }
}

void LewayK10Display::TickAnimation() {
    anim_tick_ += 1;

#if LEWAY_LANDSCAPE_EYES
    if (voice_state_ == VoiceState::Idle) {
        if (idle_ticks_ < 10000) idle_ticks_ += 1;
    } else {
        idle_ticks_ = 0;
    }

    if (wake_anim_ticks_ > 0) wake_anim_ticks_ -= 1;
    if (error_shake_ticks_ > 0) error_shake_ticks_ -= 1;

    // 1. Piscar orgânico (simples ou piscada dupla carinhosa a cada ~3 a 5.5 segundos)
    if (blink_phase_ > 0) {
        if (blink_phase_ == 1) {
            // 1 em cada 4 piscadas vira piscada dupla super fofa!
            blink_phase_ = ((anim_tick_ % 4) == 0 && idle_ticks_ < 300) ? 2 : 0;
        } else if (blink_phase_ == 2) {
            blink_phase_ = 3;  // segunda piscada rápida
        } else {
            blink_phase_ = 0;
        }
    } else if (anim_tick_ >= next_blink_tick_ && idle_ticks_ <= 900) {
        blink_phase_ = 1;
        next_blink_tick_ = anim_tick_ + 24 + (anim_tick_ % 22);
    }

    // 2. Olhar vivo pros lados (saccades espontâneas em Espera e Escuta!)
    if (anim_tick_ >= next_gaze_tick_) {
        static constexpr int kIdleGazeX[8] = {0, -16, 0, 18, -12, 14, 0, 0};
        static constexpr int kIdleGazeY[8] = {0, -4, -6, -3, 6, 4, 0, -2};
        static constexpr int kListenGazeX[6] = {0, -10, 10, 0, -6, 6};
        static constexpr int kListenGazeY[6] = {-4, -6, -6, -2, -4, -4};

        if (voice_state_ == VoiceState::Listening) {
            uint32_t idx = (anim_tick_ / 7) % 6;
            target_gaze_x_ = kListenGazeX[idx];
            target_gaze_y_ = kListenGazeY[idx];
            next_gaze_tick_ = anim_tick_ + 12 + (anim_tick_ % 10);
        } else if (voice_state_ == VoiceState::Speaking) {
            // Falando: olha direto pra você com micro-variação calorosa
            target_gaze_x_ = ((anim_tick_ % 3) == 0) ? 4 : (((anim_tick_ % 3) == 1) ? -4 : 0);
            target_gaze_y_ = -3;
            next_gaze_tick_ = anim_tick_ + 16;
        } else if (idle_ticks_ > 750) {
            // Dormindo: centraliza suave
            target_gaze_x_ = 0;
            target_gaze_y_ = 4;
            next_gaze_tick_ = anim_tick_ + 40;
        } else {
            uint32_t idx = (anim_tick_ / 5) % 8;
            target_gaze_x_ = kIdleGazeX[idx];
            target_gaze_y_ = kIdleGazeY[idx];
            next_gaze_tick_ = anim_tick_ + 20 + (anim_tick_ % 18);
        }
    }

    // Interpola olhar suavemente (inteiro puro)
    gaze_x_ = StepToward(gaze_x_, target_gaze_x_, 6);
    gaze_y_ = StepToward(gaze_y_, target_gaze_y_, 4);

    RenderLivingEyes(false);
#else
    if (blink_phase_ == 0 && anim_tick_ >= next_blink_tick_) {
        blink_phase_ = 1;
        ApplyPortraitMoodGeometry(true);
        return;
    } else if (blink_phase_ > 0) {
        blink_phase_ = 0;
        next_blink_tick_ = anim_tick_ + 14 + (anim_tick_ % 8);
        ApplyPortraitMoodGeometry(false);
    }

    static constexpr int kSpeakWave[6] = {0, 8, 16, 10, 4, 12};
    static constexpr int kListenWave[4] = {0, 6, 10, 6};

    if (voice_state_ == VoiceState::Speaking) {
        int delta = kSpeakWave[anim_tick_ % 6];
        UpdateAuraSizes(128 + delta, 100 + (delta / 2), 74 + (delta / 4));
    } else if (voice_state_ == VoiceState::Listening) {
        if ((anim_tick_ & 1U) == 0) {
            int delta = kListenWave[(anim_tick_ >> 1) % 4];
            UpdateAuraSizes(128 + delta, 100 + (delta / 2), 74);
        }
    } else {
        UpdateAuraSizes(128, 100, 74);
    }
#endif
}
