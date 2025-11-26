---@meta

TE = {}

---@return table
function TE.object_create(tilemap_name) end

---@return table
function TE.object_create(tilemap_name, extends) end

-- SOUND:
TE.sound = {}

function TE.sound.play(sound, gain, pitch) end

function TE.sound.set_loops(sound, loops) end

---@return boolean
function TE.sound.is_playing(sound) end

---@return number
function TE.sound.get_position(sound) end

function TE.sound.set_position(sound, position) end

function TE.sound.stop(sound) end

function TE.sound.stop_all() end

-- MUSIC:
TE.music = {}

function TE.music.play(sound) end

function TE.music.set_loop_points(a, b) end

function TE.music.stop() end

function TE.music.unpause() end

function TE.music.pause() end

---@return boolean
function TE.music.is_playing() end

---@return number
function TE.music.get_position() end

function TE.music.set_position(position) end

function TE.music.set_pitch(pitch) end

-- GFX:
TE.gfx = {}

-- Font Functions --
function TE.gfx.draw_font(x, y, font, _, __, string, rgba) end

-- Sprite Functions --

function TE.gfx.draw_sprite(sprite_index, image_index, x, y) end

function TE.gfx.draw_sprite_ext(sprite_index, image_index, x, y, xscale, yscale, rotation, rgba) end

function TE.gfx.draw_sprite_origin(sprite_index, image_index, x, y, xscale, yscale, origin_x, origin_y, keep_old_origin_pos, rotation, rgba) end

-- Canvas Functions --

---@return any canvas
function TE.gfx.create_canvas(width, height) end

function TE.gfx.set_canvas(canvas) end

function TE.gfx.resize_canvas(canvas, width, height) end

function TE.gfx.draw_canvas(canvas, x, y, xscale, yscale, origin_x, origin_y, angle) end

-- Primitive shapes --

function TE.gfx.draw_rectangle(x1, y1, x2, y2, rgba) end
function TE.gfx.draw_circle(x, y, r, rgba) end

-- SHADERS:
---@return any
function TE.gfx.create_shader(fragment_code, vertex_code) end
---@return any
function TE.gfx.create_fragment_shader(code) end
---@return any
function TE.gfx.create_vertex_shader(code) end
function TE.gfx.set_shader_uniform(shader, uniform, data) end
function TE.gfx.set_shader(shader) end
