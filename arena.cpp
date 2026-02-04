#include "arena.h"
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#endif

#define ARENA_BASE_POS_LOCAL ARENA_BASE_POS
#define ARENA_ALIGN_LOCAL ARENA_ALIGN

mem_arena* arena_create(u64 reserve_size, u64 commit_size) {
    u32 pagesize = plat_get_pagesize();
    reserve_size = ALIGN_UP_POW2(reserve_size, pagesize);
    commit_size = ALIGN_UP_POW2(commit_size, pagesize);

    mem_arena* arena = (mem_arena*)plat_mem_reserve(reserve_size);
    if (!arena) return nullptr;
    if (!plat_mem_commit(arena, commit_size)) {
        plat_mem_release(arena, reserve_size);
        return nullptr;
    }

    arena->reserve_size = reserve_size;
    arena->commit_size = commit_size;
    arena->pos = ARENA_BASE_POS_LOCAL;
    arena->commit_pos = commit_size;
    return arena;
}

void arena_destroy(mem_arena* arena) {
    if (!arena) return;
    plat_mem_release(arena, arena->reserve_size);
}

void* arena_push(mem_arena* arena, u64 size, bool non_zero) {
    u64 pos_aligned = ALIGN_UP_POW2(arena->pos, ARENA_ALIGN_LOCAL);
    u64 new_pos = pos_aligned + size;

    if (new_pos > arena->reserve_size) return nullptr;

    if (new_pos > arena->commit_pos) {
        u64 new_commit_pos = new_pos + arena->commit_size - 1;
        new_commit_pos -= new_commit_pos % arena->commit_size;
        new_commit_pos = MIN(new_commit_pos, arena->reserve_size);

        u8* mem = (u8*)arena + arena->commit_pos;
        u64 commit_size = new_commit_pos - arena->commit_pos;

        if (!plat_mem_commit(mem, commit_size)) return nullptr;
        arena->commit_pos = new_commit_pos;
    }

    arena->pos = new_pos;
    u8* out = (u8*)arena + pos_aligned;

    if (!non_zero) std::memset(out, 0, (size_t)size);
    return out;
}

void arena_pop(mem_arena* arena, u64 size) {
    size = MIN(size, arena->pos - ARENA_BASE_POS_LOCAL);
    arena->pos -= size;
}

void arena_pop_to(mem_arena* arena, u64 pos) {
    u64 size = (pos < arena->pos) ? (arena->pos - pos) : 0;
    arena_pop(arena, size);
}

void arena_clear(mem_arena* arena) {
    arena_pop_to(arena, ARENA_BASE_POS_LOCAL);
}

mem_arena_temp arena_temp_begin(mem_arena* arena) {
    return mem_arena_temp{ arena, arena->pos };
}

void arena_temp_end(mem_arena_temp temp) {
    arena_pop_to(temp.arena, temp.start_pos);
}

THREAD_LOCAL mem_arena* _scratch_arena[2] = { nullptr, nullptr };

mem_arena_temp arena_scratch_get(mem_arena** conflicts, u32 num_conflicts) {
    i32 scratch_index = -1;

    for (i32 i = 0; i < 2; i++) {
        bool conflict_found = false;
        for (u32 j = 0; j < num_conflicts; j++) {
            if (_scratch_arena[i] == conflicts[j]) {
                conflict_found = true;
                break;
            }
        }
        if (!conflict_found) {
            scratch_index = i;
            break;
        }
    }

    if (scratch_index == -1) return mem_arena_temp{ nullptr, 0 };

    mem_arena** selected = &_scratch_arena[scratch_index];
    if (*selected == nullptr) *selected = arena_create(MiB(64), MiB(1));

    return arena_temp_begin(*selected);
}

void arena_scratch_release(mem_arena_temp scratch) {
    arena_temp_end(scratch);
}

#ifdef _WIN32
u32 plat_get_pagesize(void) {
    SYSTEM_INFO sysinfo = { 0 };
    GetSystemInfo(&sysinfo);
    return sysinfo.dwPageSize;
}

void* plat_mem_reserve(u64 size) {
    return VirtualAlloc(nullptr, (SIZE_T)size, MEM_RESERVE, PAGE_READWRITE);
}
bool plat_mem_commit(void* ptr, u64 size) {
    return VirtualAlloc(ptr, (SIZE_T)size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
}
bool plat_mem_decommit(void* ptr, u64 size) {
    return VirtualFree(ptr, 0, MEM_DECOMMIT) != 0;
}
bool plat_mem_release(void* ptr, u64 size) {
    return VirtualFree(ptr, 0, MEM_RELEASE) != 0;
}
#endif
