/*************************************************************************************|
|   1. YOU ARE NOT ALLOWED TO SHARE/PUBLISH YOUR CODE (e.g., post on piazza or online)|
|   2. Fill main.c and memory_hierarchy.c files                                       |
|   3. Do not use any other .c files neither alter main.h or parser.h                 |
|   4. Do not include any other library files                                         |
|*************************************************************************************/
#include "mipssim.h"

/// @students: declare cache-related structures and variables here

#define CACHE_BLOCK_SIZE 16
int size_index, size_tag, size_offset, no_blocks;

struct cache_direct_mapped{
    int data[CACHE_BLOCK_SIZE];
    int tag;
    int valid;
};

struct cache_direct_mapped *cache;

void memory_state_init(struct architectural_state* arch_state_ptr) {
    arch_state_ptr->memory = (uint32_t *) malloc(sizeof(uint32_t) * MEMORY_WORD_NUM);
    memset(arch_state_ptr->memory, 0, sizeof(uint32_t) * MEMORY_WORD_NUM);
    if(cache_size == 0){
        // CACHE DISABLED
        memory_stats_init(arch_state_ptr, 0); 
    }else {
        // CACHE ENABLED
        no_blocks = cache_size / CACHE_BLOCK_SIZE;
        cache = malloc(no_blocks* sizeof(struct cache_direct_mapped));
        size_offset = ceil(log2(CACHE_BLOCK_SIZE));
        size_index = ceil(log2(no_blocks));
        size_tag = 32 - size_index - size_offset;
        memory_stats_init(arch_state_ptr, size_tag); 

    }
}

int word_reconstruct_from_bytes(int first_byte, int second_byte, int third_byte, int last_byte){
    return first_byte << 24 | second_byte << 16 | third_byte << 8 | last_byte;
}

int split_word_in_bytes(int word, int part){
    int result;
    switch(part){
        case 0:
            result = get_piece_of_a_word(word, 24, 8);
            break;
        case 1:
            result = get_piece_of_a_word(word, 16, 8);
            break;
        case 2:
            result = get_piece_of_a_word(word, 8, 8);
            break;
        case 3:
            result = get_piece_of_a_word(word, 0, 8);
            break;
        return result;
    }
}

// returns data on memory[address / 4]
int memory_read(int address){
    arch_state.mem_stats.lw_total++;
    check_address_is_word_aligned(address);

    if(cache_size == 0){
        // CACHE DISABLED
        return (int) arch_state.memory[address / 4];
    }
    else{
        // CACHE ENABLED
        int offset = get_piece_of_a_word(address, 0, 4);  
        int index = get_piece_of_a_word(address, 4, size_index);
        int tag_address = get_piece_of_a_word(address, size_index + 4, size_tag);
 
        if(cache[index].valid == 1 && cache[index].tag == tag_address ){
            arch_state.mem_stats.lw_cache_hits++;
            return word_reconstruct_from_bytes(cache[index].data[offset],cache[index].data[offset+1],cache[index].data[offset+2],cache[index].data[offset+3]);
        }
        else{
            cache[index].valid = 1;
            cache[index].tag = tag_address;


            for(int i = 0; i < 16; i++){
                int part = i % 4;
                int word = arch_state.memory[(address-offset+i) / 4];
                cache[index].data[i] = split_word_in_bytes(word, part);
            }

            return (int) arch_state.memory[address / 4];
        }

    }
    return 0;
}

// writes data on memory[address / 4]
void memory_write(int address, int write_data){
    arch_state.mem_stats.sw_total++;
    check_address_is_word_aligned(address);

    if(cache_size == 0){
        // CACHE DISABLED
        arch_state.memory[address / 4] = (uint32_t) write_data;
    }else{
        // CACHE ENABLED
        int offset = get_piece_of_a_word(address, 0, 4);  
        int index = get_piece_of_a_word(address, 4, size_index);
        int tag_address = get_piece_of_a_word(address, size_index + 4, size_tag);

        if(cache[index].valid == 1 && cache[index].tag == tag_address ){
            arch_state.mem_stats.sw_cache_hits++;
            arch_state.memory[address / 4] = (uint32_t) write_data;

            for(int i = 0; i < 4; i++){
                cache[index].data[offset+i] = split_word_in_bytes(write_data, i);
            }

        }
        else{
            arch_state.memory[address / 4] = (uint32_t) write_data;
        }
        
    }        
}
