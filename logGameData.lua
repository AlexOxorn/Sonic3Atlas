require("socket.socket")

PLANE_A_X = 0xEE78
PLANE_A_Y = 0xEE7C
PLANE_B_X = 0xEE8C
PLANE_B_Y = 0xEE90

H_INTERRUPT_COUNT = 0xF644
H_INTERRUPT_JUMP = 0x7FFFF6

H_SCROLL_BUFF = 0xE000
H_SCROLL_BUFF_SIZE = 0x380

PALETTE_WATER = 0xF080
PALETTE_NORMAL = 0xFC00
WATER_LEVEL = 0xF648

OBJ_RAM = 0xB000
OBJ_SIZE = 0x4A

SPRITE_TABLE = 0xAC00

SPRITE_CHILD_COUNT_OFFSET = 0x16

SPRITE_RENDER_START = 0x1AD20
CURRENT_ZONE = 0xFE10
CURRENT_ACT = 0xFE11
RING_LOC_PTRS = 0x1E3E58
RING_ANIMATION_FRAME = 0xFEB3
RING_STATUS_TABLE = 0xE700
RING_STATUS_TABLE_SIZE = 0xEB00 - 0xE700
CMap_Ring = 0xEBEE
CMap_Entry_Size = 0xEBF6 - CMap_Ring
CMap_Ring_Total = CMap_Entry_Size * 8
WATER_FLAG = 0xF730

Camera_min_X_pos = 0xEE14
Camera_max_X_pos = 0xEE16
Camera_min_Y_pos = 0xEE18
Camera_max_Y_pos = 0xEE1A

DMA_QUEUE_ADD_EXEC_ADDR = 0x1530

VDP_data_port = 0xC00000
VDP_control_port = 0xC00004

R_VRAM = 0
R_CRAM = 4
R_CRAM_W = 1
R_VSRAM = 2
R_VRAM_BR = 6

IS_PAUSED = 0xF63A
LAG_FRAME_COUNT = 0xF628


VDP_mock = {
    WRITE = false,
    ADDRESS = 0,
    RAM = R_VRAM,
    VtoV = false,
    DMA = false,
    CARE = false,
    BACKGROUND_COLOR = 0

    AUTO_INC = 2
}

VRAM_ADDR_CHANGED_LAST_FRAME = {}
VRAM_ADDR_CHANGED = {}

FG_EVENT = 0xEEC0
BG_EVENT = 0xEEC2

BG_EVENT_VAR = 0xEED2
FG_EVENT_VARS = {
    0xEEB4, 0xEEB6, 0xEEB8, 0xEEBE, 0xEEC4, 0xEEC6,
}
LBZ2_DeathEgg = 0xEF40
Current_zone_and_act = 0xFE10
GAME_MODE = 0xF600

currentGameMode = 0
previousGameMode = 0

function MarkVRAM()
    local addr = VDP_mock.ADDRESS
    local last = VRAM_ADDR_CHANGED[#VRAM_ADDR_CHANGED];

    if ((last ~= nil) and (addr - last[2] == VDP_mock.AUTO_INC)) then
        last[2] = addr
    else
        VRAM_ADDR_CHANGED[#VRAM_ADDR_CHANGED + 1] = {addr, addr}
    end
end

B_CD0 = 0x40000000
B_CD1 = 0x80000000
B_CD2 = 0x10
B_CD3 = 0x20
B_CD4 = 0x40
B_CD5 = 0x80
B_ADDR_L = 0x3fff0000
B_ADDR_H = 0x3

S_CD0 = 30
S_CD1 = 31
S_CD2 = 4
S_CD3 = 5
S_CD4 = 6
S_CD5 = 7

S_ADDR_L = 16
S_ADDR_H = 14

function dontCare()
    VDP_mock.CARE = false;
end


event.on_bus_write (function (addr, val, flags)
    -- Auto Inc Register
    if (val >= 0x8F00 and val < 0x9000) then
        VDP_mock.AUTO_INC = val & (0xFF)
    end

    if (val >= 0x8700 and val < 0x8800) then
        BACKGROUND_COLOR = val & 0xFF
    end

    -- CD0 = 1
    if (val < 0x40000000) then return dontCare() end

    -- VRAM = CD1-3 : 000
    if ((val & B_CD1) ~= 0) then return dontCare() end
    if ((val & B_CD2) ~= 0) then return dontCare() end
    if ((val & B_CD3) ~= 0) then return dontCare() end

    -- DMA = CD5
    if ((val & B_CD5) ~= 0) then return dontCare() end

    -- VRAM TO VRAM copy = CD4
    if ((val & B_CD4) ~= 0) then return dontCare() end

    local address_low = (val & B_ADDR_L) >> S_ADDR_L;
    local address_high = (val & B_ADDR_H) << S_ADDR_H;

    VDP_mock.CARE = true
    VDP_mock.ADDRESS = address_high + address_low
    VDP_mock.WRITE = true
    VDP_mock.RAM = R_VRAM
    VDP_mock.VtoV = false
    VDP_mock.DMA = false
end, VDP_control_port)

event.on_bus_write (function (addr, val, flags)
    if (not VDP_mock.CARE) then return end
    MarkVRAM()
    VDP_mock.ADDRESS = VDP_mock.ADDRESS + VDP_mock.AUTO_INC
end, VDP_data_port);

VRAM_AREA = {
    WINDOW = {0x8000, 0x0000},  -- 0x400 to 0x480
    SCROLL_A = {0xC000, 0xD000}, -- 0x600 to 0x680
    SCROLL_B = {0xE000, 0xF000}, -- 0x700 to 0x780
    H_SCROLL_TABLE = {0xF000, 0xF380}, -- 0x780 to 0x79C
    SAT = {0xF800, 0x200},       -- 0x7C0 to 7d0

    	-- dc.w $8004	; H-int disabled
		-- dc.w $8134	; V-int enabled, display blanked, DMA enabled, 224 line display
		-- dc.w $8230	; Scroll A PNT base $C000
		-- dc.w $8320	; Window PNT base $8000
		-- dc.w $8407	; Scroll B PNT base $E000
		-- dc.w $857C	; Sprite attribute table base $F800
		-- dc.w $8600
		-- dc.w $8700	; Backdrop color is color 0 of the first palette line
		-- dc.w $8800
		-- dc.w $8900
		-- dc.w $8A00
		-- dc.w $8B00	; Full-screen horizontal and vertical scrolling
		-- dc.w $8C81	; 40 cell wide display, no interlace
		-- dc.w $8D3C	; Horizontal scroll table base $F000
		-- dc.w $8E00
		-- dc.w $8F02	; Auto-increment is 2
		-- dc.w $9001	; Scroll planes are 64x32 cells
		-- dc.w $9100
		-- dc.w $9200	; Window disabled
}

VRAM_TILE_AREA = {
    -- [0x000] = 0x400-1,
    -- [0x480] = 0x600-1,
    -- [0x680] = 0x700-1,
    -- -- [0x79C] = 0x7C0-1,
    -- -- [0x7d0] = 0x7FF
    -- [0x780] = 0x7FF
    [0x000] = 0x100,
    [0x100] = 0x200,
    [0x200] = 0x300,
    [0x300] = 0x400,
    [0x480] = 0x500,
    [0x500] = 0x600,
    [0x680] = 0x700,
    [0x700] = 0x800,
}

function timeFunction(disp, name, func)
    local start = os.clock()
    local ret = func()
    local stop = os.clock()
    if disp then
        print(string.format("time %s: %f", name, stop-start))
    end
    return ret
end

function bytes_to_string(bytes)
	s = ""
    if (#bytes == 0) then
        return ""
    end
	for i = 1, #bytes do
	  s = s .. string.char(bytes[i])
	end
	return s
end

function readBytesToString(addr, len, scope)
    if (memory.read_bytes_as_binary_string == nil) then
        return bytes_to_string(memory.read_bytes_as_array(addr, len, scope))
    else
        return memory.read_bytes_as_binary_string(addr, len, scope)
    end
end

function int_to_bytes(int, width)
    local s = ""
    for i = 0, width-1 do
        s = s ..  string.char((int >> (i * 8)) & 0xFF)
    end
    return s
end

Connection = {}
function Connection:new(useFile, filename)
    o = {} 
    setmetatable(o, self)
    self.__index = self

    if useFile then
        client = {}
        client.file = io.open(filename, "wb")
        
        function client:send(data)
            self.file:write(data)
        end
        self.exp = event.onexit(function()
            if self.client then self.client.file:close() end
        end)
        self.client = client
    else
        while self.server == nil do
            print('trying connection')
            self.server = (socketM.bind("127.0.0.1", port or 5000))
            emu.yield()
            client.exactsleep(1000)
        end
        self.ip, self.port = self.server:getsockname()
        print(self.ip, ":", self.port)
        serv_only = event.onexit(function()
            if self.server then self.server:close() end
        end)
        
        self.server:settimeout(1)
        self.client = nil
        
        while self.client == nil do
            self.client, err = self.server:accept()
            if self.client == nil then print(err) end
            coroutine.yield()
        end
        print(self.client, err)

            event.unregisterbyid(serv_only)
        self.exp = event.onexit(function()
            if self.client then self.client:close() end
            if self.server then self.server:close() end
        end)
    end

    self.chunk_checksum = nil
    self.tile_cache = {}
    self.vram_hash_cache = {}
    for i = 1, 0x800 * 0x20 do
        self.tile_cache[i] = 0
    end
    self.block_checksum = nil
    self.level_data_checksum_A = nil
    self.level_data_checksum_B = nil

    self.current_zone = nil
    self.current_act = nil

    self.vram_updates = {}

    self.lag = false

    return o
end

function Connection:send_ring_placement()
    local zone = memory.read_u8(CURRENT_ZONE, '68K RAM')
    local act = memory.read_u8(CURRENT_ACT, '68K RAM')
    if (zone ~= self.current_zone) or (act ~= self.current_act) then
        self.current_zone = zone
        self.current_act = act

        local index = zone * 2 + act
        local ring_location = memory.read_u32_be(RING_LOC_PTRS + index * 4, 'M68K BUS')

        self.client:send("RING_POS")

        local r_index = 1
        while true do
            local x_pos = memory.read_u16_be(ring_location + r_index * 4)
            self.client:send(int_to_bytes(x_pos, 2))
            if x_pos == 0xFFFF then
                break
            end

            local y_pos = memory.read_u16_be(ring_location + r_index * 4 + 2)
            self.client:send(int_to_bytes(y_pos, 2))
            r_index = r_index + 1
        end
    end
end

function Connection:send_ring_status()
    local frame_mapping = memory.read_u8(RING_ANIMATION_FRAME, '68K RAM')
    local ring_table = readBytesToString(RING_STATUS_TABLE+2, RING_STATUS_TABLE_SIZE-2, '68K RAM')

    self.client:send('RINGSTAT')
    self.client:send(int_to_bytes(frame_mapping, 1))
    self.client:send(ring_table)
end

function Connection:send_chunks()
    local newhash = memory.hash_region(0x0000, 0x8000, '68K RAM')
    if (self.chunk_checksum == newhash) then
        return
    end

    local chunk1 = readBytesToString(0x0000, 0x8000, '68K RAM')
    self.chunk_checksum = newhash
    self.client:send('CHUNKTST')
    print(string.format("Sending: %d chunks", #chunk1))
    self.client:send(chunk1)
end

function Connection:send_palette_ram()
    color_data = readBytesToString(PALETTE_NORMAL, 0x80, '68K RAM')
    water_data = readBytesToString(PALETTE_WATER, 0x80, '68K RAM')
    self.client:send('COLORTST')
    self.client:send(color_data)
    self.client:send(water_data)

    self.client:send('BGCOLOUR')
    self.client:send(int_to_bytes(VDP_mock.BACKGROUND_COLOR, 1))
end

function Connection:send_full_vram()
    str_data = readBytesToString(0x0000, 65536, 'VRAM')
    self.client:send('TILE_TST')
    print(string.format("Sending: %d tiles", #str_data))
    self.client:send(str_data)
end

function Connection:DMAQueueAddCallback()
    local d1 = emu.getregister('M68K D1')
    local d2 = emu.getregister('M68K D2')
    local d3 = emu.getregister('M68K D3')
    local src = (d1 & 0xFFFFFF)
    local dst = d2 & 0xFFFF
    local len = 2 * (d3 & 0xFFFF)

    local srcData = readBytesToString(src, len, 'M68K BUS')
    self.vram_updates[dst] = srcData
end

function Connection:send_vram_updates()
    for i, rng in pairs(VRAM_ADDR_CHANGED) do
        local low = rng[1] // 0x20;
        if low < 2048 then
            local high = rng[2] // 0x20 + 1;
            if high >= 2047 then high = 2047 end
            local size = high - low + 1

            self.client:send('VRAM_SET')
            self.client:send(int_to_bytes(low, 2))
            self.client:send(int_to_bytes(size, 2))
            self.client:send(readBytesToString(low * 0x20, size * 0x20, 'VRAM'))
        else
            print(string.format("Cared about VRAM Addr %x?", rng[1]))
        end
    end

    for dest, data in pairs(self.vram_updates) do
        if (dest % 0x20) == 0 and (#data % 0x20) == 0 then
            self.client:send('VRAM_DMA')
            self.client:send(int_to_bytes(dest/0x20, 2))
            self.client:send(int_to_bytes(#data/0x20, 2))
            self.client:send(data)
        end
    end

    self.vram_updates = {}
    VRAM_ADDR_CHANGED = {}
end


function Connection:send_blockmap()
    local newhash = memory.hash_region(0x9000, 0xA800-0x9000, '68K RAM')
    if newhash == self.block_checksum then
        return
    end
    self.block_checksum = newhash
    str_data = readBytesToString(0x9000, 0xA800-0x9000, '68K RAM')
    self.client:send('BLOCKTST')
    print(string.format("Sending: %d blocks", #str_data))
    self.client:send(str_data)
end

function Connection:stop()
    event.unregisterbyid(self.exp)
    if self.client then self.client:close() end
    if self.server then self.server:close() end
end


function Connection:send_level_data()
    local fore_cols = memory.read_u16_be(0x8000, '68K RAM')
    local fore_rows = memory.read_u16_be(0x8004, '68K RAM')


    self.client:send('LVLDAT_A')
    self.client:send(int_to_bytes(fore_cols, 2))
    self.client:send(int_to_bytes(fore_rows, 2))

    for i = 1, fore_rows do
        row_addr = memory.read_u16_be(0x8008 + (i-1) * 4, '68K RAM')
        if (row_addr < 0x10000) then
            row = readBytesToString(row_addr, fore_cols, '68K RAM')
        else
            local zeros = {}
            for i = 1, fore_cols do
                zeros[i] = 0
            end
            row = bytes_to_string(zeros)
        end
        self.client:send(row)
    end
end

function Connection:send_level_data_sub()
    self.level_data_checksum_B = level_hash
    local back_cols = memory.read_u16_be(0x8002, '68K RAM')
    local back_rows = memory.read_u16_be(0x8006, '68K RAM')


    self.client:send('LVLDAT_B')
    self.client:send(int_to_bytes(back_cols, 2))
    self.client:send(int_to_bytes(back_rows, 2))

    for i = 1, back_rows do
        row_addr = memory.read_u16_be(0x8008 + (i-1) * 4 + 2, '68K RAM')
        row = readBytesToString(row_addr, back_cols, '68K RAM')
        self.client:send(row)
    end
end

function Connection:send_level_data_full()
    local level_hash = memory.hash_region(0x8000, 0x1000, '68K RAM')
    if self.level_data_checksum_A == level_hash then
        return true
    end

    self.level_data_checksum_A = level_hash
    self:send_level_data()
    self:send_level_data_sub()
end

function Connection:send_sprite_at(head)
    local render_data = memory.read_u8(head + 0x4, '68K RAM')
    local sonic_object_entry = readBytesToString(head, 0x4A, '68K RAM')
    local mapping_base = memory.read_u32_be(head + 0xC, '68K RAM')
    
    if (render_data & 0x20) == 0 then
        -- Dynamic Mapping
        local anim_frame = memory.read_u8(head + 0x22, '68K RAM')
        local mapping_offset = memory.read_s16_be(mapping_base + (anim_frame * 2))
        local mapping_size = memory.read_s16_be(mapping_base + mapping_offset)
        if mapping_size < 0 then

            print(string.format("Bad Mapping Data: %4x", head))
            print(string.format("mapping_base: %4x", mapping_base))
            print(string.format("anim_frame: %4x", anim_frame))
            print(string.format("mapping_offset_addr: %6x", mapping_base + (anim_frame * 2)))
            print(string.format("mapping_offset: %d", mapping_offset))
            print(string.format("mapping_size: %4x", mapping_size))
            return
        end

        local mapping_data = readBytesToString(mapping_base + mapping_offset + 2, 6 * mapping_size)
        if (#mapping_data ~= mapping_size * 6) then
            print("Bad Read %d != %d", #mapping_data, mapping_size * 6)
            return
            end
        self.client:send('NEXT')
        self.client:send(sonic_object_entry)
        self.client:send(int_to_bytes(mapping_size, 2))
        self.client:send(mapping_data)

        if (render_data & 0x40) ~= 0 then
            -- Compound Sprite
            local count_children = memory.read_u16_be(head + SPRITE_CHILD_COUNT_OFFSET, '68K RAM')
            self.client:send(int_to_bytes(count_children, 2))
            for i = 0, count_children-1 do
                local mapping_frame = memory.read_u8(
                    head + SPRITE_CHILD_COUNT_OFFSET + 2 + (6 * i) + 5, '68K RAM'
                )
                -- print(string.format("mapping child %d frame %d", i, mapping_frame))

                local mapping_offset = memory.read_s16_be(mapping_base + (mapping_frame * 2))
                local mapping_size = memory.read_u16_be(mapping_base + mapping_offset)
                local mapping_data = readBytesToString(mapping_base + mapping_offset + 2, 6 * mapping_size)

                self.client:send(int_to_bytes(mapping_size, 2))
                self.client:send(mapping_data)
            end
        end
    elseif  (render_data & 0x40) ~= 0 then
        -- Compound Static
        print(string.format("Compound Static\n"))
    else
        -- Static Mapping
        local mapping_data = readBytesToString(mapping_base, 6)
        self.client:send('NEXT')
        self.client:send(sonic_object_entry)
        self.client:send(int_to_bytes(1, 2))
        self.client:send(mapping_data)
    end
end

function Connection:send_sprite_data()
    self.client:send('SPRITE_2')

    for i = 0, 7 do
        local prio_addr = SPRITE_TABLE + 0x80 * i
        local prio_size = memory.read_u16_be(prio_addr, '68K RAM')/2
        if (prio_size <0 or prio_size >= 0x40) then
            print(string.format("prio %d addr %x size %d", i, prio_addr, prio_size))
        end

        if prio_size > 0 then
            for j = 1, prio_size do
                local head = memory.read_u16_be(prio_addr + j * 2, '68K RAM')
                self:send_sprite_at(head)
            end
        end
    end
    self.client:send('DONE')
end

function Connection:old_sprite_data()
    self.client:send('SPRITE_2')

    for i = 0, 109 do
        local head = OBJ_RAM + i * OBJ_SIZE
        local routine = memory.read_u16_be(head, '68K RAM')
        if routine ~= 0 then
            self:send_sprite_at(head)
        end
    end
    self.client:send('DONE')
end

function Connection:sonic_position()
    local x_pos = memory.read_u16_be(OBJ_RAM + 0x10, '68K RAM')
    local y_pos = memory.read_u16_be(OBJ_RAM + 0x14, '68K RAM')
    self.client:send('POSITION')
    self.client:send(int_to_bytes(x_pos,2))
    self.client:send(int_to_bytes(y_pos,2))
end

function Connection:wait_for_close()
    x = self.client:send("DONE____")
    x = self.client:receive(1)
end

function Connection:send_screen_position()
    self.client:send('SCRN_POS')
    self.client:send(int_to_bytes(memory.read_u16_be(PLANE_A_X, '68K RAM'), 2))
    self.client:send(int_to_bytes(memory.read_u16_be(PLANE_A_Y, '68K RAM'), 2))
    self.client:send(int_to_bytes(memory.read_u16_be(PLANE_B_X, '68K RAM'), 2))
    self.client:send(int_to_bytes(memory.read_u16_be(PLANE_B_Y, '68K RAM'), 2))
end

function Connection:scroll_offsets()
    self.client:send('H_SCROLL')
    self.client:send(
        readBytesToString(H_SCROLL_BUFF, H_SCROLL_BUFF_SIZE, '68K RAM')
    )
end

function Connection:waterline()
    self.client:send('WATER_LV')
    self.client:send(int_to_bytes(
        memory.read_u8(WATER_FLAG, '68K RAM'), 1
    ))
    self.client:send(int_to_bytes(
        memory.read_u16_be(WATER_LEVEL, '68K RAM'), 2
    ))
end

function Connection:wait_for_response()
    self.client:send("DONE____")
end

function Connection:ring_mappings()
    self.client:send('RING_MAP')
    self.client:send(
        readBytesToString(CMap_Ring, CMap_Ring_Total, "M68K BUS"))

end

function Connection:send_loop_data()
    self.client:send('LOOPDATA')
    self.client:send(int_to_bytes(memory.read_u16_be(0xEEAA, "68K RAM"), 2))
    self.client:send(int_to_bytes(memory.read_u16_be(Camera_min_X_pos, "68K RAM"), 2))
    self.client:send(int_to_bytes(memory.read_u16_be(Camera_min_Y_pos, "68K RAM"), 2))
    self.client:send(int_to_bytes(memory.read_u16_be(Camera_max_X_pos, "68K RAM"), 2))
    self.client:send(int_to_bytes(memory.read_u16_be(Camera_max_Y_pos, "68K RAM"), 2))
end

function Connection:send_events_data()
    self.client:send('EVNTDAT2')
    self.client:send(int_to_bytes(memory.read_u16_be(Current_zone_and_act, "68K RAM"), 2))
    self.client:send(int_to_bytes(memory.read_u16_be(BG_EVENT, "68K RAM"), 2))
    self.client:send(int_to_bytes(memory.read_u16_be(FG_EVENT, "68K RAM"), 2))
    for i=0, 8 do
        self.client:send(int_to_bytes(memory.read_u16_be(BG_EVENT_VAR + i * 2, "68K RAM"), 2))
    end
    for i=1, #FG_EVENT_VARS do
        self.client:send(int_to_bytes(memory.read_u16_be(FG_EVENT_VARS[i], "68K RAM"), 2))
    end
    self.client:send(int_to_bytes(memory.read_u16_be(LBZ2_DeathEgg, "68K RAM"), 2))
end

function Connection:send_pause_state()
    self.client:send('IS_PAUSE')
    self.client:send(int_to_bytes(memory.read_u16_be(IS_PAUSED, "68K RAM"), 2))
end


function Connection:send_lag_count()
    self.client:send('LAGCOUNT')
    self.client:send(int_to_bytes(memory.read_u16_be(LAG_FRAME_COUNT, "68K RAM"), 2))
end

function Connection:send_vram()
    timeFunction(false, 'send_pause_state', function() connection:send_pause_state() end)
    timeFunction(false, 'send_palette_ram', function() connection:send_palette_ram() end)
    timeFunction(false, 'send_loop_data', function() connection:send_loop_data() end)
    timeFunction(false, 'send_ring_placement', function() connection:send_ring_placement() end)
    timeFunction(false, 'send_ring_status', function() connection:send_ring_status() end)
	previousGameMode = currentGameMode
	currentGameMode = memory.read_u8(GAME_MODE, "68K RAM")
    if (previousGameMode & 0x80) ~= 0 and (currentGameMode & 0x80) == 0 then
        timeFunction(false, 'send_full_vram', function() connection:send_full_vram() end)
    elseif (currentGameMode & 0x80) == 0 then
        timeFunction(false, 'vram_updates', function() connection:send_vram_updates() end)
    end
    --timeFunction(false, 'tileset', function() connection:send_tileset() end)
    timeFunction(false, 'send_screen_position', function() connection:send_screen_position() end)
    timeFunction(false, 'scroll_offsets', function() connection:scroll_offsets() end)
    timeFunction(false, 'waterline', function() connection:waterline() end)
    timeFunction(false, 'blockmap', function() connection:send_blockmap() end)
    timeFunction(false, 'chunkmap', function() connection:send_chunks() end)
    if (currentGameMode == 0x0C or currentGameMode == 0x08) then
        timeFunction(false, 'level_data', function() connection:send_level_data_full() end)
    end
    timeFunction(false, 'sonic_position', function() connection:sonic_position() end)
    timeFunction(false, 'send_events_data', function() connection:send_events_data() end)
    timeFunction(false, 'send_lag_count', function() connection:send_lag_count() end)
    connection:wait_for_response()
end


connection = Connection:new(true, "03-Draft.bin")

FRAME_LOOP = 0
render_sprite = false
render_vram = false
render_else = false

connection:ring_mappings()
connection:send_full_vram()
connection:send_vram()

local c1 = event.on_bus_exec(function(addr, val, flags)
    if render_sprite then
        timeFunction(false, 'sprites', function() connection:send_sprite_data() end)
        render_sprite = false
        render_vram = true
    end
end, 0x1AD20)
local c2 = event.on_bus_exec(function(addr, val, flags)
    if render_vram then
        connection:send_vram()
        render_vram = false
    end
end, 0x15B8)
local c3 = event.on_bus_exec(function (addr, val, flags)
    connection:DMAQueueAddCallback()
end, DMA_QUEUE_ADD_EXEC_ADDR)

while true do
    timeFunction(false, 'Total Frame', function() emu.frameadvance() end)
    for i=1, FRAME_LOOP do
        emu.frameadvance()
    end
    render_sprite = true
end



-- connection:send_single_chunk()