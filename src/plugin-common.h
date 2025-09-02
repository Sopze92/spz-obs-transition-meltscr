#pragma once
#include <obs-module.h>

#include <plugin-support.h>
#include <util/platform.h>

#include <stdio.h>
#include <time.h>

#include <inttypes.h>

#define TABLESFILE_HEADER_SIZE 48

#define STATE_FLAG_DEAD 0b00000010
#define STATE_FLAG_DIRT 0b00000001

extern const uint32_t 
                min_slices,
                max_slices,
                min_steps,
                max_steps,
                max_length,
                max_size;

extern uint8_t original_values[256];

extern struct meltscr_table **tables;
extern uint16_t table_count;

#pragma pack(push, 1)
struct meltscr_table_packed {
    uint64_t uuid;
    uint16_t position;
    uint8_t death_mark;
    uint16_t values_size;
    uint16_t offsets_size;
};
#pragma pack(pop)

struct meltscr_table {
    uint64_t uuid;
    uint16_t position;
    uint8_t users;
    uint8_t state_flags;
    uint16_t values_size;
    uint16_t offsets_size;
    uint8_t 
      *_values,
      *_offsets;
    gs_texture_t *_texture;
};

static inline float lerp(float a, float b, float factor)
{
    return (1 - factor) * a + factor * b;
}

static inline int imax(int a, int b)
{
    return a > b ? a : b;
}

static inline int get_next_power_two(int value) {
    int v= 2;
    while (v < value) v = v * 2;
    return v;
}

static inline int get_next_power_two_sqrted(int value)
{
    return get_next_power_two((int)sqrt(value));
}

static inline int clamp(int v, int min, int max) {
    return v > max ? max : v < min ? min : v;
}

static void get_writable_table(struct meltscr_table_packed *out, struct meltscr_table *in)
{
    out->uuid = in->uuid;
    out->position = in->position;
    out->death_mark = in->users == 0u ? STATE_FLAG_DEAD : 0u;
    out->values_size = in->values_size;
    out->offsets_size = in->offsets_size;
}

static void get_runtime_table(struct meltscr_table *out, struct meltscr_table_packed *in, uint8_t *values, uint8_t *offsets)
{
    out->uuid = in->uuid;
    out->position = in->position;
    out->users = 0u;
    out->state_flags = (in->death_mark ? STATE_FLAG_DEAD : 0u) | STATE_FLAG_DIRT;
    out->values_size = in->values_size;
    out->offsets_size = in->offsets_size;
    out->_values = values;
    out->_offsets = offsets;
    out->_texture = NULL;
}

static struct meltscr_table *get_table_by_uuid(uint64_t uuid)
{
    //blog(LOG_INFO, "searching for table with uuid %" PRIu64 " among %u tables", uuid, table_count);
    if (uuid) {
        struct meltscr_table *table;
        for (uint16_t i = 0u; i < table_count; i++) {

            table = tables[i];

            if (!table) {
                //blog(LOG_INFO, "NULL table at index %u", i);
                continue;
            }
            //else blog(LOG_INFO, "checking table %u (uuid %" PRIu64 " &[0x%" PRIuPTR "])", i, table->uuid, table);

            if (table->uuid == uuid) return table;
        }
    }
    return NULL;
}

static uint64_t create_table()
{
    struct meltscr_table *table = (struct meltscr_table *)bzalloc(sizeof(struct meltscr_table));
    uint64_t uuid = (uint64_t)time(NULL);

    table->uuid = uuid;
    table->users = 0u;
    table->_values = bmalloc(max_size);
    table->_offsets = bmalloc(max_size);
    table->state_flags = STATE_FLAG_DIRT | STATE_FLAG_DEAD;

    struct meltscr_table **newtables = bzalloc(sizeof(struct meltscr_table *) * (table_count + 1));

    if (table_count > 0u) {
        memcpy(newtables, tables, (size_t)table_count * sizeof(struct meltscr_table *));
        bfree(tables);
    }

    newtables[table_count] = table;

    tables = newtables;
    table_count++;

    //blog(LOG_INFO, "allocated new table with uuid %llu for index %u at &[0x%llx]", uuid, table_count - 1, table);
    //obs_log(LOG_INFO, "total tables count: %u", table_count);

    return uuid;
}

static void leave_table(struct meltscr_table *table)
{
    if (table->users > 0u) table->users--;
    else obs_log(LOG_WARNING, "table with uuid %" PRIu64 " x[0x%" PRIxPTR "] already had 0 users", table->uuid, table);
}

static void join_table(struct meltscr_table *table)
{
    table->users++;
    table->state_flags &= ~STATE_FLAG_DEAD;
}

static void generate_table_values(struct meltscr_table *table)
{
    uint8_t *values = table->_values;
    uint16_t size = table->values_size;

    for (uint16_t i = 0u; i < size; i++) values[i] = rand() & 0xFF;

    // DEBUG ONLY

    //obs_log(LOG_INFO, "result:");
    //
    //int rsize = (int)sqrt(size);
    //int ssize = (int)pow(rsize*3, 2) + 1;
    //char *text = bzalloc((size_t)rsize*4);

    //{
    //    int idx;
    //    for (int i = 0; i < (int)rsize; i++) {
    //        idx = 0;
    //        for (int j = 0; j < (int)rsize; j++) {
    //            idx += sprintf(&text[idx], "%02x ", values[i * rsize + j] & 0xFF);
    //        }
    //        blog(LOG_INFO, "%s", text);
    //        text[0] = '\0';
    //    }
    //}

    //bfree(text);
}

static void generate_table_offsets(struct meltscr_table *table, int steps, float increment, float factor)
{
    uint8_t *values = table->_values;
    uint8_t *offsets = table->_offsets;
    uint16_t size = table->values_size;
    uint16_t slices = table->offsets_size;

    uint8_t maxstep = steps - 1;
    uint8_t stepsize = (uint8_t)round(255.0 * factor / maxstep);

    uint8_t inc_value = (uint8_t)imax(1, (int)round(steps * increment));
    uint8_t inc_modulo = (uint8_t)imax(3, inc_value * 2 + 1);

    uint16_t pos = table->position;

    offsets[0] = (uint8_t)(values[pos] % steps);

    int8_t prev;
    
    for (uint16_t i = 1; i < slices; i++) {

        prev = offsets[i - 1];

        pos++;
        if (pos == size) pos = 0u;

        offsets[i] = (uint8_t)clamp(prev - ((values[pos] % inc_modulo) - inc_value), 0, maxstep);
    }

    table->position = pos;

    for (int i = 0; i < slices; i++) offsets[i] *= stepsize;

    // DEBUG ONLY

    //obs_log(LOG_INFO, "result:");
    //
    //int rsize = (int)sqrt(size);
    //int ssize = (int)pow(rsize * 3, 2) + 1;
    //char *text = bzalloc((size_t)rsize * 4);
    //
    //{
    //    int idx;
    //    for (int i = 0; i < (int)rsize; i++) {
    //        idx = 0;
    //        for (int j = 0; j < (int)rsize; j++) {
    //            idx += sprintf(&text[idx], "%02x ", offsets[i * rsize + j] & 0xFF);
    //        }
    //        blog(LOG_INFO, "%s", text);
    //        text[0] = '\0';
    //    }
    //}
    //
    //bfree(text);
}

static void write_tables_to_disk()
{
    char *tables_path = obs_module_config_path("TABLES1.WAD");

    FILE *f = os_fopen(tables_path, "wb");
    if (f != NULL) {

        //blog(LOG_INFO, "writing %u table(s) to disk", table_count);

        char notice[TABLESFILE_HEADER_SIZE] = {0};
        memcpy(notice, "|-This file cannot be read in HUMAN mode.", 41);
        memcpy(&notice[TABLESFILE_HEADER_SIZE - 2], "-|", 2);

        fwrite(notice, TABLESFILE_HEADER_SIZE, 1, f);
        fwrite(&table_count, 2, 1, f);

        if (table_count > 0u) {

            size_t table_disk_size = sizeof(struct meltscr_table_packed);

            struct meltscr_table_packed *table_disk = bzalloc(table_disk_size);
            struct meltscr_table *table;

            for (uint16_t i = 0u; i < table_count; i++) {

                table = tables[i];

                if (table->users == 0u && (table->state_flags & STATE_FLAG_DEAD) != 0) {
                    //obs_log(LOG_INFO, "table with uuid %" PRIu64 " removed due cleanup", table->uuid);
                    continue;
                }

                get_writable_table(table_disk, table);
                fwrite(table_disk, table_disk_size, 1, f);

                fwrite(table->_values, max_slices, 1, f);

                blog(LOG_INFO, "written table with uuid %" PRIu64, table->uuid);
            }

            bfree(table_disk);
        }

        fclose(f);
    }
    else obs_log(LOG_ERROR, "IO Error writting tables: unable to write to file");

    bfree(tables_path);
}

static void read_tables_from_disk()
{
    char *tables_path = obs_module_config_path("TABLES1.WAD");

    FILE *f = os_fopen(tables_path, "rb");
    if (f != NULL) {

        char message[256] = {0};

        size_t rlen;
        bool success = true;
        uint32_t tables_valid = 0;

        fseek(f, TABLESFILE_HEADER_SIZE, SEEK_SET);
        rlen= fread(&table_count, 2, 1, f);

        if (rlen != 1) {
            success = false;
            sprintf(message, "IO Error reading table list");
        }
        else if (table_count > 0u) {

            //blog(LOG_INFO, "reading %i tables.", table_count);

            size_t table_disk_size = sizeof(struct meltscr_table_packed);
            struct meltscr_table_packed *table_disk = bzalloc(table_disk_size);

            tables = bzalloc(sizeof(struct meltscr_table *) * table_count);
            size_t table_size = sizeof(struct meltscr_table);
            struct meltscr_table *table;

            for (uint16_t i = 0u; i < table_count; i++) {

                rlen= fread(table_disk, table_disk_size, 1, f);
                if (rlen != 1) {
                    success = false;
                    sprintf(message, "IO Error on table at index %u", i);
                    tables_valid = i;
                    break;
                }

                uint8_t *values = bmalloc(max_size);

                rlen= fread(values, max_slices, 1, f);
                if (rlen != 1) {
                    success = false;
                    sprintf(message, "IO Error on buffer values of table at index %u", i);
                    bfree(values);
                    tables_valid = i;
                    break;
                }

                table = bmalloc(table_size);
                uint8_t *offsets = bmalloc(max_size);

                get_runtime_table(table, table_disk, values, offsets);

                blog(LOG_INFO, "read table with uuid %" PRIu64, table->uuid);

                tables[(int)i] = table;
            }

            bfree(table_disk);
        }
            
        if(!success) table_count = tables_valid;

        //else blog(LOG_INFO, "no tables to read.");

        fclose(f);

        if (!success) {
            obs_log(LOG_ERROR, "Error occurred while reading the tables file at '%s'.", tables_path);
            blog(LOG_ERROR, "%s", message);
            blog(LOG_INFO, "Sadly, all the unreadable table buffers will be regenerated...");
        }


    }

    bfree(tables_path);
}

static inline float cubic_ease_in_out(float t)
{
    if (t < 0.5f) return 4.0f * t * t * t;

    float _tmp = (2.0f * t - 2.0f);
    return (t - 1.0f) * _tmp * _tmp + 1.0f;
}
