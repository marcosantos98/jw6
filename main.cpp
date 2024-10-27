#include <algorithm>
#include <cmath>
#include <initializer_list>

#include <raylib.h>
#include <raymath.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#include "arena.hpp"
#include "da.hpp"

// :define
#define v2(x, y) Vector2{float(x), float(y)}
#define v2of(val) v2(val, val)
#define v4(x, y, z, w) (Vector4){x, y, z, w}
#define v4v2(a, b) Vector4{a.x, a.y, b.x, b.y}
#define v4zw(z, w) Vector4{0, 0, z, w}
#define xyv4(v) v2(v.x, v.y)
#define to_rect(_v4) (Rectangle){_v4.x, _v4.y, _v4.z, _v4.w}
#define to_v2(v) v2(v.x, v.y)
#define to_v4(r) v4(r.x, r.y, r.width, r.height)

#define ZERO v2of(0)

typedef void* rawptr;

struct Ctx {
	GrowingArena main;
	GrowingArena temp;
};
static Ctx ctx = {};

struct Time {
	int h, m, s;
};

Time seconds_to_hm(int seconds) {
	Time t = {};
	t.h = int(seconds / 3600);
	t.m = int(seconds - (3600 * t.h)) / 60;
	t.s = int(seconds - (3600 * t.h) - (t.m * 60));
	return t;
}

Vector4 grow(Vector4 old, float amt) {
	return {
		old.x - amt, 
		old.y - amt,
		old.z + amt * 2, 
		old.w + amt * 2,
	};
}

void start_of(Vector4 where, Vector4* it) {
	it->x = where.x;
	it->y = where.y;
}

void end_of(Vector4 where, Vector4* it) {
	it->x = where.x + where.z;
	it->y = where.y;
}

void bottom_of(Vector4 where, Vector4* it) {
	it->y = where.y + where.w - it->w;
}

void center(Vector4 where, Vector4* it, int axis) {
	switch (axis) {
		case 0:
			it->x += (where.z - it->z) * 0.5f;
			break;
		case 1:
			it->y += (where.w - it->w) * .5f;
	}
}

enum Side {
	TOP,
	BOTTOM,
	LEFT,
	RIGHT,
};

void pad(Vector4* it, Side side, float amt) {
	switch(side) {
		case TOP:
			it->y += amt;
			break;
		case BOTTOM:
			it->y -= amt;
			break;
		case LEFT:
			it->x += amt;
			break;
		case RIGHT:
			it->x -= amt;
			break;
	}
}

void below(Vector4 where, Vector4* it) {
	it->y = where.y + where.w;
}

float scale(float xMax, float y, float yMax) {
	return xMax * (y / yMax);
}

int signd(int x) {
    return (x > 0) - (x < 0);
}

float approach(float current, float target, float increase) {
    if (current < target) {
        return fmin(current + increase, target);
    }
    return fmax(current - increase, target);
}

Rectangle rv2(const Vector2& pos, const Vector2& size) {
	return {pos.x, pos.y, size.x, size.y};
}

Vector4 operator*(const Vector4& a, const Vector4& b) {
	return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; 
}

const Vector2 operator*(const Vector2& a, const Vector2& b) {
	return Vector2Multiply(a, b);
}

const Vector2 operator+(const Vector2& a, const float b) {
	return Vector2Add(a, {b, b});
}

const Vector2 operator+(const Vector2& a, const Vector2& b) {
	return Vector2Add(a, b);
}

const Vector2 operator-(const Vector2& a, const Vector2& b) {
	return Vector2Subtract(a, b);
}

const Vector2 operator*(const Vector2& a, const float b) {
	return Vector2Multiply(a, {b, b});
}

const Vector2 operator/(const Vector2& a, const Vector2& b) {
	return Vector2Divide(a, b);
}

const Vector2 operator/(const Vector2& a, const float b) {
	return Vector2Divide(a, {b, b});
}

const Vector2 v2_floor(const Vector2& a) {
	return {floorf(a.x), floorf(a.y)};
}

const Vector2 WINDOW_SIZE = v2(948, 533);
const Vector2 RENDER_SIZE = v2(640, 360);

// :renderer
typedef enum DrawObjType {
	NONE,
	DRAW_OBJ_QUAD,
	DRAW_OBJ_TEXTURE,
	DRAW_OBJ_TEXTURE_PRO,
	DRAW_QUAD_LINES,
	DRAW_OBJ_TEXT,
	DRAW_OBJ_CIRCLE,
} DrawObjType;

struct DrawObj {
	DrawObjType type;
	Vector4 src;
	Vector4 dest;
	Color tint;
	float line_tick;
	Texture2D tex;
	const char* text;
	float text_size;
	f32 radius;
	Font f;
};

struct RenderLayer {
	daa<DrawObj> objs;
};

#define MAX_LAYERS 1024
struct Renderer {
	RenderLayer layers[MAX_LAYERS];	
	Texture2D atlas;
	int current_layer;
	daa<int> layer_stack;

	void push_layer(int layer) {
		layer_stack.append(current_layer);
		current_layer = layer;
	}

	void pop_layer() {
		current_layer = layer_stack.pop();
	}

	void add(DrawObj obj) {
		layers[current_layer].objs.append(obj);
	}
};
Renderer* renderer = NULL;

void renderer_init(Renderer* renderer) {
	memset(renderer->layers, 0, sizeof(RenderLayer) * MAX_LAYERS);
	for (int i = 0; i < MAX_LAYERS; i++) {
		RenderLayer* layer = &renderer->layers[i];
		layer->objs = make<DrawObj>(&ctx.temp, 8);
	}
	renderer->layer_stack = make<int>(&ctx.main);
	renderer->current_layer = 0;
}

void draw_circle(Vector2 center, f32 radius, Color color = WHITE) {
	renderer->add({
		.type = DRAW_OBJ_CIRCLE,
		.dest = v4(center.x, center.y, 0, 0),
		.tint = color,
		.radius = radius,
	});
}

void draw_text(Font f, Vector2 dest, const char* text, float text_size, Color tint = WHITE) {
	renderer->add({
		.type = DRAW_OBJ_TEXT,
		.dest = v4(dest.x, dest.y, 0, 0),
		.tint = tint,
		.text = text,
		.text_size = text_size,
		.f = f,
	});
}

void draw_quad(Vector4 dest, Color tint = WHITE) {
	renderer->add({
		.type = DRAW_OBJ_QUAD,
		.dest = dest,
		.tint = tint,
	});
}

void draw_quad_lines(Vector4 dest, float line_tick = 1.f, Color tint = WHITE) {
	renderer->add({
		.type = DRAW_QUAD_LINES,
		.dest = dest,
		.tint = tint,
		.line_tick = line_tick,
	});
}

void draw_texture_v2(Vector4 src, Vector2 pos, Color tint = WHITE) {
	renderer->add({
		.type = DRAW_OBJ_TEXTURE,
		.src = src,
		.dest = {pos.x, pos.y, 0, 0},
		.tint = tint,
	});
}

void draw_texture_pro(Texture2D texture, Vector4 src, Vector4 dest, Color tint = WHITE) {
	renderer->add({
		.type = DRAW_OBJ_TEXTURE_PRO,
		.src = src,
		.dest = dest,
		.tint = tint,
		.tex = texture,
	});	
}

void flush_renderer() {
	for(int i = 0; i < MAX_LAYERS; i++) {
		RenderLayer* layer = &renderer->layers[i];
		for (int j = 0; j < layer->objs.count; j++) {
			DrawObj it = layer->objs.items[j];
			switch (it.type) {
				case NONE:
					break;
				case DRAW_OBJ_QUAD:
					DrawRectangleRec(to_rect(it.dest), it.tint);
					break;
				case DRAW_OBJ_TEXTURE:
					DrawTextureRec(renderer->atlas, to_rect(it.src), {it.dest.x, it.dest.y}, it.tint);
					break;
				case DRAW_QUAD_LINES:
					DrawRectangleLinesEx(to_rect(it.dest), it.line_tick, it.tint);
					break;
				case DRAW_OBJ_TEXTURE_PRO:
					DrawTexturePro(it.tex, to_rect(it.src), to_rect(it.dest), ZERO, 0, it.tint);
					break;
				case DRAW_OBJ_TEXT:
					DrawTextEx(it.f, it.text, xyv4(it.dest), it.text_size, 2, it.tint);
					break;
				case DRAW_OBJ_CIRCLE:
					DrawCircleV(xyv4(it.dest), it.radius, it.tint);
					break;
			}	
		}
		layer->objs.count = 0;
	}
	assert(renderer->layer_stack.count == 0 && "unclosed layers!");
}
// ;renderer

// :entity

enum EntityId {
	EID_NONE,
};

enum EntityType {
	ET_NONE,
	ET_PROJECTILE,
	ET_ENEMY,
	// :type
};

enum EntityProp {
	EP_NONE,
};

struct Entity {
	int handle;
	Vector2 pos, vel, size, remainder;
	EntityId id;
	EntityType type;
	daa<EntityProp> props;
	bool valid;
	bool grounded;
	Entity* last_collided;
	rawptr user_data;
	float facing;
	Entity* riding;
	bool trigger;
	bool was_selected;
	f32 health;
	bool attacked;
	Entity* target;
};

void en_add_props(Entity* entity, std::initializer_list<EntityProp> props) {
	for(EntityProp prop : props) {
		entity->props.append(prop);
	}
}

bool en_has_prop(Entity en, EntityProp prop) {
	for (int i = 0; i < en.props.count; i++) {
		if (en.props.items[i] == prop) {
			return true;
		}
	}
	return false;
}

void en_setup(Entity* en, Vector2 pos, Vector2 size) {
	en->pos = pos;
	en->remainder = ZERO;
	en->vel = ZERO;
	en->size = size;
	en->valid = true;
	en->props = make<EntityProp>(&ctx.main, 8);
}

Rectangle en_box(Entity en) {
	return rv2(en.pos, en.size);
}

Vector2 en_center(Entity en) {
	return {en.pos.x + en.size.x / 2, en.pos.y + en.size.y / 2};
}

void en_invalidate(Entity* en) {
	memset(en, 0, sizeof(Entity));	
}

// ;entity

// :data
#define PLAYER_SHOOT_TIMER 1.2f
#define ENEMY_HEALTH 10
#define ENEMY_AGRO 200
#define ENEMY_SPAWN_ENEMY_TIME 1.1f
#define PLAYER_MAX_HEALTH 10

#define MAX_ENTITIES 2046 
struct State {
	Entity entities[MAX_ENTITIES];
	Entity* player;
	Vector2 virtual_mouse;
	Camera2D cam;
	RenderTexture main_pass;
	RenderTexture ui_pass;
	f32 enemy_spawn_timer;
	i32 score;
	f32 timer;
	Font font;
	Sound shoot;
	Sound hit;
	Sound die;
	bool end;
};
State *state = NULL;

Entity* new_en() {
	for(int i = 0; i < MAX_ENTITIES; i++) {
		if (!state->entities[i].valid) {
			state->entities[i].handle = i;
			return &state->entities[i];
		}
	}
	assert(true && "Ran out of entities");
	return nullptr;
}

daa<Entity*> get_all_with_type(EntityType type) {
	daa<Entity*> with = make<Entity*>(&ctx.temp);

	for(int i = 0; i < MAX_ENTITIES; i++) {
		Entity* en = &state->entities[i];
		if (!en->valid) continue;
		if (en->type == type) {
			with.append(en);
		}
	}

	return with;
}

// :projectile
Entity* projectile_init(Vector2 pos, Vector2 dir) {
	Entity* en = new_en();

	en_setup(en, pos, v2of(10));

	en->type = ET_PROJECTILE;

	en->vel = dir * 1000.f;

	return en;
}

void projectile_update(Entity* self) {
	self->pos.x += self->vel.x * GetFrameTime();
	self->pos.y += self->vel.y * GetFrameTime();

	daa<Entity*> enemies = get_all_with_type(ET_ENEMY);
	for(int i = 0; i < enemies.count; i++) {
		Entity* en = enemies.items[i];
		if(CheckCollisionRecs(en_box(*self), en_box(*en))) {
			en->valid = false;
			self->valid = false;
			state->score += 1;
			PlaySound(state->die);
			return;
		}
	}
}

void projectile_render(Entity self) {
	draw_quad(to_v4(en_box(self)));
}
// ;projectile
// :player

struct PlayerData {
	f32 shoot_timer;
	f32 shoot_time;
	f32 player_speed;
};

Entity* player_init() {
	Entity* en = new_en();

	en_setup(en, ZERO, v2of(10));

	PlayerData* data = arena_alloc<PlayerData>(&ctx.main, sizeof(PlayerData));
	data->shoot_time = PLAYER_SHOOT_TIMER;
	data->player_speed = 100;
	en->user_data = data;

	en->health = PLAYER_MAX_HEALTH;

	return en;
}

void player_update(Entity* self) {
	
	PlayerData* data = (PlayerData*)self->user_data;
	self->vel = v2of(data->player_speed);

	if (IsKeyDown(KEY_A)) {
		self->pos.x -= self->vel.x * GetFrameTime();
	} else if (IsKeyDown(KEY_D)) {
		self->pos.x += self->vel.x * GetFrameTime();
	}
	
	if (IsKeyDown(KEY_W)) {
		self->pos.y -= self->vel.y * GetFrameTime();
	} else if (IsKeyDown(KEY_S)) {
		self->pos.y += self->vel.y * GetFrameTime();
	}


	data->shoot_timer -= GetFrameTime();
	if (data->shoot_timer < 0) {
		Vector2 mouse_pos = state->virtual_mouse;
		Vector2 in_world = GetScreenToWorld2D(mouse_pos, state->cam);
		Vector2 dir = Vector2Normalize(Vector2Subtract(in_world, self->pos));
		projectile_init(self->pos, dir);
		data->shoot_timer = data->shoot_time;
		PlaySound(state->shoot);
	}

	data->player_speed += 0.01f * GetFrameTime();
	data->shoot_time -= 0.001f * GetFrameTime();

	data->player_speed = fminf(data->player_speed, 120.f);
	data->shoot_time = fmaxf(data->shoot_time, 0.7f);
}

void player_render(Entity self) {
	draw_quad(to_v4(en_box(self)), RED);
}

// ;player
// :enemy

struct EnemyData {
	bool has_agro;
};

Entity* enemy_init(Vector2 pos) {
	Entity* en = new_en();

	en_setup(en, pos, v2of(20));

	en->health = ENEMY_HEALTH;
	en->type = ET_ENEMY;

	return en;
}

void enemy_update(Entity* self) {
	self->pos = Vector2MoveTowards(self->pos, state->player->pos, 100 * GetFrameTime());

	if (CheckCollisionRecs(en_box(*self), en_box(*state->player))) {
		state->player->health -= 1;
		self->valid = false;
		PlaySound(state->hit);
	}
}

void enemy_render(Entity self) {
	draw_quad(to_v4(en_box(self)), GOLD);
	draw_quad_lines(to_v4(en_box(self)), 2.f, RED);
}
// ;enemy

enum Layer {
	L_NONE,
};

bool ui_btn(Vector2 pos, const char* text, float text_size, bool can_click = true) {

	Vector4 dest = v4zw(96, 32);
	dest.x = pos.x;
	dest.y = pos.y;

	bool hover = false;
	bool clicked = false;

	if (CheckCollisionPointRec(state->virtual_mouse, to_rect(dest))) {
		hover = true;
		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
			clicked = true;
		}
	}

	float text_sz = MeasureText(text, text_size);
	Vector2 text_pos = { 
		(dest.z - text_sz) * .5f, 
		(dest.w - text_size) * .5f,
	};

	text_pos = xyv4(dest) + text_pos;

	draw_texture_v2({128, hover ? 240.f : 208.f, 96, 32}, pos);
	draw_text(state->font, text_pos, text, text_size);

	if(!can_click) {
		draw_quad(v4(pos.x, pos.y, 96, 32), ColorAlpha(GRAY, .8));
	}

	return clicked && can_click;
}

void update_frame() {
	float scale = fmin(WINDOW_SIZE.x / RENDER_SIZE.x, WINDOW_SIZE.y / RENDER_SIZE.y);
	state->virtual_mouse = (GetMousePosition() - (WINDOW_SIZE - (RENDER_SIZE * scale)) * .5) / scale;
	state->virtual_mouse = Vector2Clamp(state->virtual_mouse, ZERO, RENDER_SIZE);	
	// :update
	{
		if (!state->end) {
			state->timer += GetFrameTime();

			player_update(state->player);

			state->cam.target = state->player->pos;

			// :spawn
			{
				state->enemy_spawn_timer -= GetFrameTime();
				if (state->enemy_spawn_timer < 0) {
					Vector2 pos = v2(
							state->player->pos.x + GetRandomValue(-200, 200),
							state->player->pos.y + GetRandomValue(-200, 200)
					);
					enemy_init(pos);
					state->enemy_spawn_timer = ENEMY_SPAWN_ENEMY_TIME;
				}
			}
			// :entities
			for(int i = 0; i < MAX_ENTITIES; i++) {
				Entity* en = &state->entities[i];
				if (!en->valid) continue;
				switch (en->type) {
				case ET_NONE: break;
				case ET_PROJECTILE: projectile_update(en); break;
				case ET_ENEMY: enemy_update(en); break;
				}
			}
		}
		// :gamestate
		{
			if (state->player->health <= 0) {
				state->end = true;
			}
		}

		
	}

	// :main_pass
	{
		BeginTextureMode(state->main_pass);
		
		ClearBackground(BLANK);

		BeginMode2D(state->cam);
		{
			draw_texture_v2(v4(25, 0, 206, 104), {-103, -52});

			player_render(*state->player);

			//:entities
			for(int i = 0; i < MAX_ENTITIES; i++) {
				Entity en = state->entities[i];
				if (!en.valid) continue;
				switch (en.type) {
					case ET_NONE: break;
					case ET_PROJECTILE: projectile_render(en); break;
					case ET_ENEMY: enemy_render(en); break;
				}
			}

			draw_texture_v2({0, 0, 16, 16}, GetScreenToWorld2D(state->virtual_mouse, state->cam));

			flush_renderer();
		}
		EndMode2D();

		EndTextureMode();
	}

	// :ui_pass
	{
		BeginTextureMode(state->ui_pass);

		ClearBackground(BLANK);
		
		if (!state->end) {
			Vector2 start_pos = {(WINDOW_SIZE.x - (WINDOW_SIZE.x * .7f)) * .5f, 10.f};
			draw_quad({start_pos.x, start_pos.y, WINDOW_SIZE.x * .7f, 25.f});
			f32 scaled = (WINDOW_SIZE.x * .7f) * (state->player->health / PLAYER_MAX_HEALTH);
			draw_quad({start_pos.x, start_pos.y, scaled, 25.f}, RED);
					
			Time t = seconds_to_hm(state->timer);	
			const char * text = TextFormat("%02d:%02d:%02d", t.h, t.m, t.s);
			Vector2 text_size = MeasureTextEx(state->font, text, 24, 2);
			draw_text(state->font, v2(
						(WINDOW_SIZE.x - text_size.x) * .5f,
						WINDOW_SIZE.y - 20 - 10
						), text, 24);
		} else {
			const char* text = TextFormat("The end");
			Vector2 size = MeasureTextEx(state->font, text, 24, 2);
			draw_text(state->font, {(WINDOW_SIZE.x - size.x) * .5f, 50}, text, 24);

			text = TextFormat("Killed squares: %d", state->score);
			size = MeasureTextEx(state->font, text, 24, 2);
			draw_text(state->font, {floorf((WINDOW_SIZE.x - size.x) * .5f), 50 + 24 + 12}, text, 24);

			Time t = seconds_to_hm(state->timer);	
			text = TextFormat("Survived: %02d:%02d:%02d", t.h, t.m, t.s);
			size = MeasureTextEx(state->font, text, 24, 2);
			draw_text(state->font, {floorf((WINDOW_SIZE.x - size.x) * .5f), 50 + 24 + 12 + 24 + 12}, text, 24);
		}
		flush_renderer();

		EndTextureMode();
	}

	RenderTexture final = state->main_pass;

	BeginDrawing();
	{
		ClearBackground(BLACK);
		
		if (!state->end) {
			float scale = fminf(float(GetScreenWidth()) / RENDER_SIZE.x, float(GetScreenHeight()) / RENDER_SIZE.y);
			DrawTexturePro(
				final.texture,
				{0, 0, float(final.texture.width), float(-final.texture.height)},
				{
					(float(GetScreenWidth()) - (RENDER_SIZE.x * scale)) * 0.5f,
					(float(GetScreenHeight()) - (RENDER_SIZE.y * scale)) * 0.5f,
					RENDER_SIZE.x * scale,
					RENDER_SIZE.y * scale,
				},
				{},
				0,
				WHITE
			);
		}
		DrawTexturePro(state->ui_pass.texture, {0, 0, f32(state->ui_pass.texture.width), f32(-state->ui_pass.texture.height)}, {0, 0, WINDOW_SIZE.x, WINDOW_SIZE.y}, {}, 0, WHITE);

	
		DrawFPS(10, WINDOW_SIZE.y - 20);
	}
	EndDrawing();

	arena_reset(&ctx.temp);
}

int main(void) {

	ctx = {};

	// :raylib
	SetTraceLogLevel(LOG_WARNING);
	InitWindow(WINDOW_SIZE.x, WINDOW_SIZE.y, "jw6");
	InitAudioDevice();
	SetTargetFPS(60);
	SetExitKey(KEY_Q);
	DisableCursor();	
	
	state = (State*)arena_alloc<State>(&ctx.main, sizeof(State));
	
	// :load
	Texture2D atlas = LoadTexture("./res/atlas.png");
	state->font = LoadFontEx("./res/arial.ttf", 24, NULL, 0);
	state->main_pass = LoadRenderTexture(RENDER_SIZE.x, RENDER_SIZE.y);
	state->ui_pass = LoadRenderTexture(WINDOW_SIZE.x, WINDOW_SIZE.y);

	state->shoot = LoadSound("./res/shoot.wav");
	state->die = LoadSound("./res/die.wav");
	state->hit = LoadSound("./res/hit.wav");

	// :init
	renderer = arena_alloc<Renderer>(&ctx.main, sizeof(Renderer));
	renderer_init(renderer);
	renderer->atlas = atlas;
	
	memset(state->entities, 0, sizeof(Entity) * MAX_ENTITIES);
	state->cam = Camera2D{};
	state->cam.zoom = 1.f;
	state->cam.offset = RENDER_SIZE / v2of(2);
	state->enemy_spawn_timer = 0;
	state->score = 0;
	state->end = false;
	state->timer = 0;
	
	state->player = player_init();

	assert(renderer != NULL && "arena returned null");

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(update_frame, 60, 1);
#else
    while (!WindowShouldClose()) {
    	update_frame();
	}
#endif

	CloseWindow();

	return 0;
}
