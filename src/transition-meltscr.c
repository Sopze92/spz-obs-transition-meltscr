#include <obs-module.h>
#include <graphics/vec2.h>
#include "plugin-common.h"

#include <plugin-support.h>

#define S_PRIV_TABLEUUID "uuid"

#define S_PROP_USEORIGINAL "use_original"
#define S_PROP_SLICES "slices"
#define S_PROP_FACTOR "factor"
#define S_PROP_STEPS "steps"
#define S_PROP_INCREMENT "increment"
#define S_PROP_DIRECTION "direction"
#define S_PROP_RANDOMTYPE "random_type"
#define S_PROP_RESOLUTION "table_size"
#define S_PROP_SWAPPOINT "swap_point"
#define S_PROP_AUDIOMODE "audio_mode"

#define S_BTN_REFRESHTABLE "table_refresh"
#define S_BTN_HELP "help"

#define S_PROPGRP_FIXEDTABLE "grp_fixed_table"

struct meltscr_info {
    obs_source_t *source;
    gs_effect_t *effect;

    gs_eparam_t 
      *a_tex,
      *b_tex,
      *c_tex,
      *factor,
      *sizes,
      *dir,
      *dir_mask,
      *progress;

    bool _use_original;

    int _slices;
    float _factor;
    int _steps;
    float _increment;
    struct vec2 _dir;

    int _table_type;

    int _noise_resolution;
    int _tex_resolution;
    struct meltscr_table *_table_ptr;

    uint8_t _slice_offsets;

    int _audio_mode;
    float _audio_swap_point;
};

static void meltscr_table_mark_dirty(void *data)
{
    struct meltscr_info *dwipe = data;
    struct meltscr_table *table = dwipe->_table_ptr;
    if (table) table->state_flags |= STATE_FLAG_DIRT;
}

static void meltscr_create_table(void* data)
{
    struct meltscr_info* dwipe = data;
    struct meltscr_table *table = dwipe->_table_ptr;

    if (table) {

        //blog(LOG_INFO, "generating values for table %llu &[0x%llx]", table->uuid, dwipe->_table_ptr);

        uint8_t flags = table->state_flags;

        uint16_t slices = (uint16_t)dwipe->_slices;

        int slices_resolution = get_next_power_two_sqrted(slices);

        int resolution = dwipe->_noise_resolution == -1 ? slices_resolution : dwipe->_noise_resolution;

        dwipe->_tex_resolution = max(slices_resolution, resolution);
        table->values_size = (uint16_t)(resolution * resolution);
        table->offsets_size = slices;

        table->position = 2;

        if (dwipe->_table_type == 0) memcpy(table->_values, original_values, 256);
        else generate_table_values(table);

        obs_log(LOG_INFO, "Saving tables...");
        write_tables_to_disk();
    }
}

static void meltscr_create_texture(void *data)
{
    struct meltscr_info *dwipe = data;
    struct meltscr_table *table = dwipe->_table_ptr;

    if (table) {

        //blog(LOG_INFO, "generating offsets+texture for table %llu &[0x%llx]", table->uuid, dwipe->_table_ptr);

        generate_table_offsets(table, (uint16_t)dwipe->_steps, dwipe->_increment, dwipe->_factor);

        if (table->_texture) gs_texture_destroy(table->_texture);

        int resolution = dwipe->_tex_resolution;
        obs_enter_graphics();
        table->_texture = gs_texture_create(resolution, resolution, GS_R8, 1, &table->_offsets, 0);
        obs_leave_graphics();
    }
}

void* meltscr_create(obs_data_t *settings, obs_source_t *source)
{
    struct meltscr_info *dwipe;
    gs_effect_t *effect;

    char *file = obs_module_file("spz-meltscr-transition.effect");

    obs_enter_graphics();
    effect = gs_effect_create_from_file(file, NULL);
    obs_leave_graphics();

    bfree(file);

    if (!effect) {
        blog(LOG_ERROR, "Could not find or compile the HLSL shader 'spz-meltscr-transition.effect'");
        return NULL;
    }

    dwipe = bzalloc(sizeof(*dwipe));

    dwipe->_table_type = -1;
    dwipe->_table_ptr = NULL;

    dwipe->_tex_resolution = 1;

    dwipe->source = source;
    dwipe->effect = effect;

    dwipe->a_tex = gs_effect_get_param_by_name(effect, "tex_a");
    dwipe->b_tex = gs_effect_get_param_by_name(effect, "tex_b");
    dwipe->c_tex = gs_effect_get_param_by_name(effect, "tex_c");

    dwipe->factor = gs_effect_get_param_by_name(effect, "factor");
    dwipe->sizes = gs_effect_get_param_by_name(effect, "sizes");
    dwipe->dir = gs_effect_get_param_by_name(effect, "dir");
    dwipe->dir_mask = gs_effect_get_param_by_name(effect, "dir_mask");
    dwipe->progress = gs_effect_get_param_by_name(effect, "progress");

    obs_data_set_default_int(settings, S_PRIV_TABLEUUID, 0LL);

    // default values are DooM values
    obs_data_set_default_bool(settings, S_PROP_USEORIGINAL, false);
    obs_data_set_default_int(settings, S_PROP_SLICES, 160);
    obs_data_set_default_int(settings, S_PROP_FACTOR, 60);
    obs_data_set_default_int(settings, S_PROP_STEPS, 16);
    obs_data_set_default_int(settings, S_PROP_INCREMENT, 25);
    obs_data_set_default_int(settings, S_PROP_DIRECTION, 2);
    obs_data_set_default_int(settings, S_PROP_RANDOMTYPE, 0);
    obs_data_set_default_int(settings, S_PROP_RESOLUTION, 16);
    obs_data_set_default_int(settings, S_PROP_AUDIOMODE, 3);
    obs_data_set_default_int(settings, S_PROP_SWAPPOINT, 50);

    obs_source_update(source, settings);

    return dwipe;
}

#pragma region -------------------------------------------------------------------------------------- VIDEO

void meltscr_video_start(void *data)
{
    struct meltscr_info *dwipe = data;

    if (dwipe->_table_type == 2) meltscr_create_table(data);
    meltscr_create_texture(data);
}

static void meltscr_video_callback(void *data, gs_texture_t *a, gs_texture_t *b, float t, uint32_t cx, uint32_t cy)
{
    struct meltscr_info *dwipe = data;

    float _factor = dwipe->_factor;
    struct vec2 factor = {_factor, 1.0f / _factor};
    struct vec4 sizes = {(float)dwipe->_slices, (float)dwipe->_tex_resolution, 1.0f / dwipe->_slices, 1.0f / dwipe->_tex_resolution};

    struct vec2 dir = dwipe->_dir;
    struct vec3 dir_mask = { fabsf(dir.x), fabsf(dir.y), lerp(dir.x, dir.y, fabsf(dir.y))};

    struct vec2 progress = {t, t * (1.0f + _factor)};

    const bool previous = gs_framebuffer_srgb_enabled();
    gs_enable_framebuffer_srgb(true);

    gs_effect_set_texture_srgb(dwipe->a_tex, a);
    gs_effect_set_texture_srgb(dwipe->b_tex, b);

    gs_effect_set_vec2(dwipe->factor, &factor);
    gs_effect_set_vec4(dwipe->sizes, &sizes);
    gs_effect_set_vec2(dwipe->dir, &dir);
    gs_effect_set_vec3(dwipe->dir_mask, &dir_mask);

    if (dwipe->_table_ptr) gs_effect_set_texture(dwipe->c_tex, dwipe->_table_ptr->_texture);

    gs_effect_set_vec2(dwipe->progress, &progress);

    while (gs_effect_loop(dwipe->effect, "MeltScreen")) {
        gs_draw_sprite(NULL, 0, cx, cy);
    }

    gs_enable_framebuffer_srgb(previous);
}

void meltscr_video_render(void *data, gs_effect_t *effect)
{
    struct meltscr_info *dwipe = data;
    obs_transition_video_render(dwipe->source, meltscr_video_callback);
    UNUSED_PARAMETER(effect);
}

#pragma endregion

#pragma region -------------------------------------------------------------------------------------- AUDIO

static float meltscr_audio_callback_a(void *data, float t)
{
    struct meltscr_info *dwipe = data;
    switch (dwipe->_audio_mode) {
      default: return 1.0f - cubic_ease_in_out(t);
      case 1: return 1.0f - t;
      case 2: return t < dwipe->_audio_swap_point ? 1.0f : .0f;
      case 3: return .0f;
    }
}

static float meltscr_audio_callback_b(void *data, float t)
{
    struct meltscr_info *dwipe = data;
    switch (dwipe->_audio_mode) {
      default: return cubic_ease_in_out(t);
      case 1: return t;
      case 2: return t < dwipe->_audio_swap_point ? .0f : 1.0f;
      case 3: return floorf(t);
    }
}

bool meltscr_audio_render(void *data, uint64_t *ts_out, struct obs_source_audio_mix *audio, uint32_t mixers, size_t channels, size_t sample_rate)
{
    struct meltscr_info *dwipe = data;
    return obs_transition_audio_render(dwipe->source, ts_out, audio, mixers, channels, sample_rate, meltscr_audio_callback_a, meltscr_audio_callback_b);
}

#pragma endregion

#pragma region -------------------------------------------------------------------------------------- SETTINGS


static void meltscr_update(void *data, obs_data_t *settings)
{
    struct meltscr_info *dwipe = data;
    uint64_t buffer_uuid = (uint64_t)obs_data_get_int(settings, S_PRIV_TABLEUUID);

    bool update_table = false;

    // table assignment

    if (!buffer_uuid || !get_table_by_uuid(buffer_uuid))
    {
        if (buffer_uuid) blog(LOG_WARNING, "missing table with uuid %llu, needs to be rebuilt", buffer_uuid);

        buffer_uuid = create_table();
        obs_data_set_int(settings, S_PRIV_TABLEUUID, (int64_t)buffer_uuid);

        update_table = true;
    }

    struct meltscr_table *ctable = get_table_by_uuid(buffer_uuid);

    if (dwipe->_table_ptr != ctable) {

        if (!dwipe->_table_ptr) blog(LOG_INFO, "assigning table with uuid %llu &[0x%llx]", buffer_uuid, ctable);
        else {
            blog(LOG_INFO, "reassigning table with uuid %llu at &[0x%llx] (from &[0xllx])", buffer_uuid, ctable, dwipe->_table_ptr);
            leave_table(dwipe->_table_ptr);
        }

        dwipe->_table_ptr = ctable;
        join_table(dwipe->_table_ptr);
    } 
    //else blog(LOG_INFO, "using table with uuid %llu &[0x%llx]", buffer_uuid, ctable);

    //

    const bool use_original = obs_data_get_bool(settings, S_PROP_USEORIGINAL);

    if (use_original != dwipe->_use_original) {
        //blog(LOG_INFO, "switching to %s settings", use_original ? "original" : "custom");
        dwipe->_use_original = use_original;
    }
    else if (use_original) return;

    if (use_original) {
        dwipe->_dir = (struct vec2){.0f, 1.0f};
        dwipe->_slices = 160;
        dwipe->_factor = .6f;
        dwipe->_steps = 16;
        dwipe->_increment = .0625f;
        dwipe->_table_type = 0;
        dwipe->_noise_resolution = 16;
        dwipe->_audio_mode = 3;

        update_table= true;

    } 
    else {

        const int dir = (int)obs_data_get_int(settings, S_PROP_DIRECTION);

        switch (dir) {
          case 0: dwipe->_dir = (struct vec2){.0f, -1.0f}; break; // UP
          case 1: dwipe->_dir = (struct vec2){1.0f, .0f}; break; // RIGHT
          case 2: dwipe->_dir = (struct vec2){.0f, 1.0f}; break; // DOWN
          case 3: dwipe->_dir = (struct vec2){-1.0f, .0f}; break; // LEFT
        }

        const int 
            slices = (int)obs_data_get_int(settings, S_PROP_SLICES),
            steps = (int)obs_data_get_int(settings, S_PROP_STEPS),
            type = (int)obs_data_get_int(settings, S_PROP_RANDOMTYPE),
            resolution = (int)obs_data_get_int(settings, S_PROP_RESOLUTION),
            audio_mode = (int)obs_data_get_int(settings, S_PROP_AUDIOMODE);

        const float 
            factor = .01f * (int)obs_data_get_int(settings, S_PROP_FACTOR), 
            increment = .0025f * (int)obs_data_get_int(settings, S_PROP_INCREMENT);

        update_table = slices != dwipe->_slices || factor != dwipe->_factor || steps != dwipe->_steps || increment != dwipe->_increment || type != dwipe->_table_type || resolution != dwipe->_noise_resolution;

        dwipe->_slices = slices;
        dwipe->_factor = factor;
        dwipe->_steps = steps;
        dwipe->_increment = increment;
        dwipe->_table_type = type;
        dwipe->_noise_resolution = resolution;

        dwipe->_audio_mode = audio_mode;

        if (audio_mode == 2) {
            dwipe->_audio_swap_point = .01f * (int)obs_data_get_int(settings, S_PROP_SWAPPOINT);
        }
    }

    if (update_table) {

        struct meltscr_table *table = dwipe->_table_ptr;
        if(table && table->state_flags & STATE_FLAG_DIRT) meltscr_create_table(data);
    }
}

static bool prop_changed_use_original_callback(void *data, obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
    obs_property_t *p;

    // set basically all properties disabled if we are in original mode
    const bool enable_props = !obs_data_get_bool(settings, S_PROP_USEORIGINAL);

    p = obs_properties_get(props, S_PROP_SLICES);
    obs_property_set_enabled(p, enable_props);

    p = obs_properties_get(props, S_PROP_FACTOR);
    obs_property_set_enabled(p, enable_props);

    p = obs_properties_get(props, S_PROP_STEPS);
    obs_property_set_enabled(p, enable_props);

    p = obs_properties_get(props, S_PROP_INCREMENT);
    obs_property_set_enabled(p, enable_props);

    p = obs_properties_get(props, S_PROP_DIRECTION);
    obs_property_set_enabled(p, enable_props);

    p = obs_properties_get(props, S_PROP_RANDOMTYPE);
    obs_property_set_enabled(p, enable_props);

    p = obs_properties_get(props, S_PROP_RESOLUTION);
    obs_property_set_enabled(p, enable_props);

    p = obs_properties_get(props, S_BTN_REFRESHTABLE);
    obs_property_set_enabled(p, enable_props);

    p = obs_properties_get(props, S_PROP_AUDIOMODE);
    obs_property_set_enabled(p, enable_props);

    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(property);
    return true;
}

static bool list_changed_random_mode_callback(void *data, obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
    obs_property_t *p;

    // set TableSize dropdown and Refreshtable button only visible if current RandomMode is Fixed
    const bool fixed_visible = obs_data_get_int(settings, S_PROP_RANDOMTYPE) == 1;

    p = obs_properties_get(props, S_PROPGRP_FIXEDTABLE);
    obs_property_set_visible(p, fixed_visible);

    meltscr_table_mark_dirty(data);

    UNUSED_PARAMETER(property);
    return true;
}

static bool list_changed_table_size_callback(void *data, obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
    meltscr_table_mark_dirty(data);

    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(property);
    UNUSED_PARAMETER(settings);
    return false;
}

static bool button_pressed_refresh_table_callback(obs_properties_t *props, obs_property_t *property, void *data)
{
    meltscr_create_table(data);

    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(property);
    return false;
}

static bool list_changed_audio_mode_callback(void *data, obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
    obs_property_t *p;

    const bool swap_visible = obs_data_get_int(settings, S_PROP_AUDIOMODE) == 2;

    p = obs_properties_get(props, S_PROP_SWAPPOINT);
    obs_property_set_visible(p, swap_visible);

    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(property);
    return true;
}

static obs_properties_t *meltscr_properties(void *data, void* type_data)
{
    blog(LOG_INFO, "meltscr_properties()");

    struct meltscr_info *dwipe = data;

    obs_properties_t *props = obs_properties_create();
    obs_property_t *p;

    p= obs_properties_add_bool(props, S_PROP_USEORIGINAL, obs_module_text("UseOriginal"));
    obs_property_set_modified_callback2(p, prop_changed_use_original_callback, data);

    obs_properties_add_int_slider(props, S_PROP_SLICES, obs_module_text("Slices"), min_slices, max_slices, 1);
    obs_properties_add_int_slider(props, S_PROP_FACTOR, obs_module_text("Factor"), 1, 100, 1);
    obs_properties_add_int_slider(props, S_PROP_STEPS, obs_module_text("Steps"), min_steps, max_steps, 1);
    obs_properties_add_int_slider(props, S_PROP_INCREMENT, obs_module_text("Increment"), 1, 100, 1);
    
    p= obs_properties_add_list(props, S_PROP_DIRECTION, obs_module_text("Direction"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(p, obs_module_text("Direction.Up"), 0);
    obs_property_list_add_int(p, obs_module_text("Direction.Right"), 1);
    obs_property_list_add_int(p, obs_module_text("Direction.Down"), 2);
    obs_property_list_add_int(p, obs_module_text("Direction.Left"), 3);

    p= obs_properties_add_list(props, S_PROP_RANDOMTYPE, obs_module_text("RandomMode"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(p, obs_module_text("RandomMode.Doom"), 0);
    obs_property_list_add_int(p, obs_module_text("RandomMode.Fixed"), 1);
    obs_property_list_add_int(p, obs_module_text("RandomMode.Dynamic"), 2);
    obs_property_set_modified_callback2(p, list_changed_random_mode_callback, data);

    // --
    obs_properties_t *props2 = obs_properties_create();
    p= obs_properties_add_list(props2, S_PROP_RESOLUTION, obs_module_text("TableSize"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(p, obs_module_text("TableSize.Auto"), -1);
    obs_property_list_add_int(p, "64", 8);
    obs_property_list_add_int(p, "256", 16);
    obs_property_list_add_int(p, "1024", 32);
    obs_property_list_add_int(p, "4096", 64);
    obs_property_set_modified_callback2(p, list_changed_table_size_callback, data);

    obs_properties_add_button2(props2, S_BTN_REFRESHTABLE, obs_module_text("RefreshTable"), &button_pressed_refresh_table_callback, data);

    obs_properties_add_group(props, S_PROPGRP_FIXEDTABLE, obs_module_text("FixedTableGroup"), OBS_GROUP_NORMAL, props2);
    // --

    p= obs_properties_add_list(props, S_PROP_AUDIOMODE, obs_module_text("AudioMode"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(p, obs_module_text("AudioMode.Smooth"), 0);
    obs_property_list_add_int(p, obs_module_text("AudioMode.Linear"), 1);
    obs_property_list_add_int(p, obs_module_text("AudioMode.Swap"), 2);
    obs_property_list_add_int(p, obs_module_text("AudioMode.Mute"), 3);
    obs_property_set_modified_callback2(p, list_changed_audio_mode_callback, data);

    obs_properties_add_int_slider(props, S_PROP_SWAPPOINT, obs_module_text("SwapPoint"), 1, 100, 1);

    p = obs_properties_add_button(props, S_BTN_HELP, obs_module_text("Help"), NULL);
    obs_property_button_set_type(p, OBS_BUTTON_URL);
    obs_property_button_set_url(p, "https://sopze.com/obs-meltscr-transition/help");

    UNUSED_PARAMETER(type_data);
    return props;
}

#pragma endregion

#pragma region -------------------------------------------------------------------------------------- INTERNAL

static const char *meltscr_get_name(void *type_data)
{
    UNUSED_PARAMETER(type_data);
    return obs_module_text("MeltScrTransition");
}

void meltscr_destroy(void *data)
{
    struct meltscr_info *dwipe = data;
    if (dwipe->_table_ptr) leave_table(dwipe->_table_ptr);

    bfree(dwipe);
}

static enum gs_color_space meltscr_video_get_color_space(void *data, size_t count, const enum gs_color_space *preferred_spaces)
{
    UNUSED_PARAMETER(count);
    UNUSED_PARAMETER(preferred_spaces);

    struct meltscr_info *const dwipe = data;
    return obs_transition_video_get_color_space(dwipe->source);
}

struct obs_source_info meltscr_transition = {
    .id = "meltscr_transition",
    .type = OBS_SOURCE_TYPE_TRANSITION,
    .get_name = meltscr_get_name,
    .create = meltscr_create,
    .destroy = meltscr_destroy,
    .update = meltscr_update,
    .transition_start = meltscr_video_start,
    .video_render = meltscr_video_render,
    .audio_render = meltscr_audio_render,
    .get_properties2 = meltscr_properties,
    .video_get_color_space = meltscr_video_get_color_space,
};

#pragma endregion