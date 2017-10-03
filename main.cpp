#include "load_save_png.hpp"
#include "GL.hpp"
#include "Meshes.hpp"
#include "Scene.hpp"
#include "read_chunk.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <map>
#include <utility>

static GLuint compile_shader(GLenum type, std::string const& source);
static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader);

int main(int argc, char** argv) {
	// Configuration:
	struct {
		std::string title = "Game 3";
		glm::uvec2 size = glm::uvec2(640, 480);
	} config;

	//------------  initialization ------------

	// Initialize SDL library:
	SDL_Init(SDL_INIT_VIDEO);

	// Ask for an OpenGL context version 3.3, core profile, enable debug:
	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	// create window:
	SDL_Window* window =
			SDL_CreateWindow(config.title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, config.size.x,
											 config.size.y, SDL_WINDOW_OPENGL /*| SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI*/
											 );

	if (!window) {
		std::cerr << "Error creating SDL window: " << SDL_GetError() << std::endl;
		return 1;
	}

	// Create OpenGL context:
	SDL_GLContext context = SDL_GL_CreateContext(window);

	if (!context) {
		SDL_DestroyWindow(window);
		std::cerr << "Error creating OpenGL context: " << SDL_GetError() << std::endl;
		return 1;
	}

#ifdef _WIN32
	// On windows, load OpenGL extensions:
	if (!init_gl_shims()) {
		std::cerr << "ERROR: failed to initialize shims." << std::endl;
		return 1;
	}
#endif

	// Set VSYNC + Late Swap (prevents crazy FPS):
	if (SDL_GL_SetSwapInterval(-1) != 0) {
		std::cerr << "NOTE: couldn't set vsync + late swap tearing (" << SDL_GetError() << ")." << std::endl;
		if (SDL_GL_SetSwapInterval(1) != 0) {
			std::cerr << "NOTE: couldn't set vsync (" << SDL_GetError() << ")." << std::endl;
		}
	}

	// Hide mouse cursor (note: showing can be useful for debugging):
	// SDL_ShowCursor(SDL_DISABLE);

	//------------ opengl objects / game assets ------------

	// shader program:
	GLuint program = 0;
	GLuint program_Position = 0;
	GLuint program_Normal = 0;
	GLuint program_Color = 0;
	GLuint program_mvp = 0;
	GLuint program_itmv = 0;
	GLuint program_to_light = 0;
	{	// compile shader program:
		GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER,
																					"#version 330\n"
																					"uniform mat4 mvp;\n"
																					"uniform mat3 itmv;\n"
																					"in vec4 Position;\n"
																					"in vec3 Normal;\n"
																					"in vec4 Color;\n"
																					"out vec3 normal;\n"
																					"out vec4 color;\n"
																					"void main() {\n"
																					"	gl_Position = mvp * Position;\n"
																					"	normal = itmv * Normal;\n"
																					" color = Color;\n"
																					"}\n");

		GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER,
																						"#version 330\n"
																						"uniform vec3 to_light;\n"
																						"in vec3 normal;\n"
																						"in vec4 color;\n"
																						"out vec4 fragColor;\n"
																						"void main() {\n"
																						"	float light = max(0.0, dot(normalize(normal), to_light));\n"
																						"	fragColor = vec4(light * vec3(color), 1.0);\n"
																						"}\n");

		program = link_program(fragment_shader, vertex_shader);

		// look up attribute locations:
		program_Position = glGetAttribLocation(program, "Position");
		if (program_Position == -1U)
			throw std::runtime_error("no attribute named Position");
		program_Normal = glGetAttribLocation(program, "Normal");
		if (program_Normal == -1U)
			throw std::runtime_error("no attribute named Normal");
		program_Color = glGetAttribLocation(program, "Color");
		if (program_Color == -1U)
			throw std::runtime_error("no attribute named Color");

		// look up uniform locations:
		program_mvp = glGetUniformLocation(program, "mvp");
		if (program_mvp == -1U)
			throw std::runtime_error("no uniform named mvp");
		program_itmv = glGetUniformLocation(program, "itmv");
		if (program_itmv == -1U)
			throw std::runtime_error("no uniform named itmv");

		program_to_light = glGetUniformLocation(program, "to_light");
		if (program_to_light == -1U)
			throw std::runtime_error("no uniform named to_light");
	}

	//------------ meshes ------------

	Meshes meshes;

	{	// add meshes to database:
		Meshes::Attributes attributes;
		attributes.Position = program_Position;
		attributes.Normal = program_Normal;
		attributes.Color = program_Color;

		meshes.load("meshes.blob", attributes);
	}

	//------------ scene ------------

	Scene menuScene;
	Scene scene;
	// set up camera parameters based on window:
	scene.camera.fovy = glm::radians(60.0f);
	scene.camera.aspect = float(config.size.x) / float(config.size.y);
	scene.camera.near = 0.01f;
	//(transform will be handled in the update function below)

	// add some objects from the mesh library:
	auto add_object = [&](std::string const& name, glm::vec3 const& position, glm::quat const& rotation,
												glm::vec3 const& scale) -> Scene::Object& {
		Mesh const& mesh = meshes.get(name);
		scene.objects.emplace_back();
		Scene::Object& object = scene.objects.back();
		object.list_pos = --scene.objects.end();
		object.transform.position = position;
		object.transform.rotation = rotation;
		object.transform.scale = scale;
		object.vao = mesh.vao;
		object.start = mesh.start;
		object.count = mesh.count;
		object.program = program;
		object.program_mvp = program_mvp;
		object.program_itmv = program_itmv;
		return object;
	};

	struct Collidable {
		std::string name;
		Scene::Object* obj;
		float rad;
		glm::vec2 vel = glm::vec2(0.0f);
		float speed = 0.0f;
		float maxSpeed = 0.0f;
		float angle = 0.0f;
		// lazy implementation; only balls use this property
		Collidable* lastTouchedBy = nullptr;

		// lazy implementation; only players use this property
		int score = 0;

		Collidable(const std::string& name, Scene::Object* obj, float rad, float maxSpeed = 0.0f)
				: name(name), obj(obj), rad(rad), maxSpeed(maxSpeed) {
			angle = glm::roll(obj->transform.rotation);
		}

		bool contains(const glm::vec3& point) { return glm::distance(obj->transform.position, point) <= rad; }
		bool contains(const Collidable& other) {
			return glm::distance(obj->transform.position, other.obj->transform.position) <= rad + other.rad;
		}
		void setVelocity(float velocity) {
			velocity = glm::clamp(velocity, -3.0f, 3.0f);

			vel.x = std::sin(angle) * velocity;
			vel.y = std::cos(angle) * velocity;
		}
		void setAngle(float newAngle) {
			angle = glm::radians(newAngle);

			vel.x = std::sin(angle) * glm::length(vel);
			vel.y = std::cos(angle) * glm::length(vel);

			obj->transform.rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
		}
		void add(float dv, float da = 0.0f) {
			float velocity = glm::clamp(glm::length(vel) + std::fabs(dv), -0.5f, 0.5f);

			angle += glm::radians(da);
			obj->transform.rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));

			if (dv < 0.0f) {
				vel.x = std::cos(angle + 3.14159f) * velocity;
				vel.y = std::sin(angle + 3.14159f) * velocity;
			} else {
				vel.x = std::cos(angle) * velocity;
				vel.y = std::sin(angle) * velocity;
			}
		}
		void move(float dt) { obj->transform.position += glm::vec3(vel * dt, 0.0f); }
		void bounceOffWall() {
			glm::vec3 world = obj->transform.make_local_to_world() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

			if ((world.x > 3.0f - rad && vel.x > 0.0f) || (world.x < -3.0f + rad && vel.x < 0.0f)) {
				vel.x = -vel.x;
			}

			if ((world.y > 2.0f - rad && vel.y > 0.0f) || (world.y < -2.0f + rad && vel.y < 0.0f)) {
				vel.y = -vel.y;
			}
		}
	};
	std::vector<Collidable> goals;
	std::vector<Collidable> players;
	std::vector<Collidable> balls;

	const float BALL_RAD = 0.1525f;
	const float GOAL_RAD = 0.4f - BALL_RAD;
	const float BALL_MAX_SPEED = 3.0f;
	const float PLAYER_RAD = 0.165f;
	const float PLAYER_MAX_SPEED = 7.5f;
	{	// read objects to add from "scene.blob":
		std::ifstream file("scene.blob", std::ios::binary);

		std::vector<char> strings;
		// read strings chunk:
		read_chunk(file, "str0", &strings);

		{	// read scene chunk, add meshes to scene:
			struct SceneEntry {
				uint32_t name_begin, name_end;
				uint32_t parent_name_begin, parent_name_end;
				glm::vec3 position;
				glm::quat rotation;
				glm::vec3 scale;
			};
			static_assert(sizeof(SceneEntry) == 4 * 14, "Scene entry should be packed");

			std::vector<SceneEntry> data;
			read_chunk(file, "scn0", &data);

			for (auto const& entry : data) {
				if (!(entry.name_begin <= entry.name_end && entry.name_end <= strings.size())) {
					throw std::runtime_error("index entry has out-of-range name begin/end");
				}
				std::string name(&strings[0] + entry.name_begin, &strings[0] + entry.name_end);
				Scene::Object* obj = &add_object(name, entry.position, entry.rotation, entry.scale);
				// this is a hack instead of correcting/adding to blender output script
				if (name == "Cylinder.005") {
					goals.emplace_back(name, obj, GOAL_RAD);
				} else if (name.at(4) == '-') {
					balls.emplace_back(name, obj, BALL_RAD, BALL_MAX_SPEED);
				} else if (name == "Player.001") {
					players.emplace_back(name, obj, PLAYER_RAD, PLAYER_MAX_SPEED);
				}
			}
		}
	}

	Collidable p1 = players.at(0);
	Collidable p2 = players.at(1);

	glm::vec2 mouse = glm::vec2(0.0f, 0.0f);	// mouse position in [-1,1]x[-1,1] coordinates

	struct {
		float radius = 2.0f;
		float elevation = 1.5f;
		float azimuth = 0.0f;
		glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
	} camera;

	std::cout << "Player 1 (a,s,z,x controls) is targeting stripes." << std::endl;
	std::cout << "Player 2 (;,',.,/ controls) is targeting solids." << std::endl;
	std::cout << "The game ends when the 8 ball has been hit in, so be careful!" << std::endl;

	bool gameOver = false;

	int count = 0;
	const uint8_t* keys = SDL_GetKeyboardState(&count);
	uint8_t prev[count];
	std::memcpy(prev, keys, sizeof prev);

	//------------ game loop ------------

	bool should_quit = false;
	while (true) {
		static SDL_Event evt;
		while (SDL_PollEvent(&evt) == 1) {
			// handle input:
			if (evt.type == SDL_MOUSEMOTION) {
				// glm::vec2 old_mouse = mouse;
				mouse.x = (evt.motion.x + 0.5f) / float(config.size.x) * 2.0f - 1.0f;
				mouse.y = (evt.motion.y + 0.5f) / float(config.size.y) * -2.0f + 1.0f;
				if (evt.motion.state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
					// disabled for now
					// camera.elevation += -2.0f * (mouse.y - old_mouse.y);
					// camera.azimuth += -2.0f * (mouse.x - old_mouse.x);
				}
			} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
			} else if (evt.type == SDL_KEYDOWN) {
				if (evt.key.keysym.sym == SDLK_ESCAPE) {
					should_quit = true;
				}
			} else if (evt.type == SDL_QUIT) {
				should_quit = true;
				break;
			}
		}
		if (should_quit)
			break;

		auto current_time = std::chrono::high_resolution_clock::now();
		static auto previous_time = current_time;
		float elapsed = std::chrono::duration<float>(current_time - previous_time).count();
		previous_time = current_time;

		static float total = 0;
		total += elapsed;

		{	// update game state:
			bool p1frontLeft = keys[SDL_SCANCODE_A];
			bool p2frontLeft = prev[SDL_SCANCODE_SEMICOLON];
			bool p1frontRight = keys[SDL_SCANCODE_S];
			bool p2frontRight = keys[SDL_SCANCODE_APOSTROPHE];
			bool p1backLeft = keys[SDL_SCANCODE_Z];
			bool p2backLeft = keys[SDL_SCANCODE_PERIOD];
			bool p1backRight = keys[SDL_SCANCODE_X];
			bool p2backRight = keys[SDL_SCANCODE_SLASH];

			// a+x / s+z rotates in place
			// a / z / s / x pivot
			// a+s / z+x move normally

			static const float ACCEL = 5.0f;
			static const float ANGLE_ACCEL = 90.0f;

			static auto controls = [&](Collidable& p, bool frontLeft, bool frontRight, bool backLeft, bool backRight) {
				float dVel = 0.0f;
				float dAngle = 0.0f;
				if (frontLeft) {
					dVel += ACCEL * elapsed;
					dAngle += -ANGLE_ACCEL * elapsed;
				}
				if (frontRight) {
					dVel += ACCEL * elapsed;
					dAngle += ANGLE_ACCEL * elapsed;
				}

				if (backLeft) {
					dVel += -ACCEL * elapsed;
					dAngle += ANGLE_ACCEL * elapsed;
				}
				if (backRight) {
					dVel += -ACCEL * elapsed;
					dAngle += -ANGLE_ACCEL * elapsed;
				}
				p.add(dVel, dAngle);
			};
			controls(p1, p1frontLeft, p1frontRight, p1backLeft, p1backRight);
			controls(p2, p2frontLeft, p2frontRight, p2backLeft, p2backRight);

			if (p1.contains(p2) &&	// second part may be superfluous
					glm::dot(glm::vec3(p1.vel - p2.vel, 0.0f), p1.obj->transform.position - p2.obj->transform.position) < 0.0f) {
				p1.vel = -p1.vel;
				p2.vel = -p2.vel;
			}

			p1.bounceOffWall();
			p2.bounceOffWall();

			p1.move(elapsed);
			p2.move(elapsed);

			p1.vel *= 0.5f;
			p2.vel *= 0.5f;

			for (auto ball = balls.begin(); ball != balls.end();) {
				// friction; could be better
				ball->vel *= 0.9999f;

				if (glm::length(ball->vel) > 0.0001f) {
					glm::vec2 u = ball->vel / glm::length(ball->vel);
					float amount = glm::length(ball->vel);
					ball->obj->transform.rotation =
							glm::rotate(ball->obj->transform.rotation, glm::radians(amount), glm::vec3(u.y, u.x, 0.0f));
				}

				ball->bounceOffWall();
				ball->move(elapsed);

				if (ball->contains(p1)) {
					glm::vec3 v1 = glm::vec3(ball->vel, 0.0f);
					glm::vec3 v2 = glm::vec3(p1.vel, 0.0f);
					glm::vec3 x1 = ball->obj->transform.position;
					glm::vec3 x2 = p1.obj->transform.position;
					// https://math.stackexchange.com/a/1438040
					float movingToward = glm::dot(v1 - v2, x1 - x2);
					if (movingToward < 0.0f) {
						ball->vel = v1 - glm::dot(v1 - v2, x1 - x2) / (glm::length(x1 - x2) * glm::length(x1 - x2)) * (x1 - x2);
						// give player more power
						if (glm::length(ball->vel) < 0.5f) {
							ball->vel = ball->vel / glm::length(ball->vel) * 0.5f;
						}

						ball->lastTouchedBy = &p1;
					}
				}

				if (ball->contains(p2)) {
					glm::vec3 v1 = glm::vec3(ball->vel, 0.0f);
					glm::vec3 v2 = glm::vec3(p2.vel, 0.0f);
					glm::vec3 x1 = ball->obj->transform.position;
					glm::vec3 x2 = p2.obj->transform.position;
					float movingToward = glm::dot(v1 - v2, x1 - x2);
					if (movingToward < 0.0f) {
						ball->vel = v1 - glm::dot(v1 - v2, x1 - x2) / (glm::length(x1 - x2) * glm::length(x1 - x2)) * (x1 - x2);
						// give player more power
						if (glm::length(ball->vel) < 0.5f) {
							ball->vel = ball->vel / glm::length(ball->vel) * 0.5f;
						}

						ball->lastTouchedBy = &p2;
					}
				}

				for (Collidable& otherBall : balls) {
					if (&(*ball) == &otherBall) {
						continue;
					}
					if (ball->contains(otherBall)) {
						glm::vec3 v1 = glm::vec3(ball->vel, 0.0f);
						glm::vec3 v2 = glm::vec3(otherBall.vel, 0.0f);
						glm::vec3 x1 = ball->obj->transform.position;
						glm::vec3 x2 = otherBall.obj->transform.position;
						float movingToward = glm::dot(v2 - v1, x2 - x1);
						if (movingToward < 0.0f) {
							// https:// en.wikipedia.org/wiki/Elastic_collision#Two-dimensional_collision_with_two_moving_objects
							ball->vel = v1 - glm::dot(v1 - v2, x1 - x2) / (glm::length(x1 - x2) * glm::length(x1 - x2)) * (x1 - x2);
							otherBall.vel =
									v2 - glm::dot(v2 - v1, x2 - x1) / (glm::length(x2 - x1) * glm::length(x2 - x1)) * (x2 - x1);

							// no good way to break ties here, maybe use speed?
							if (ball->lastTouchedBy) {
								otherBall.lastTouchedBy = ball->lastTouchedBy;
							} else {
								ball->lastTouchedBy = otherBall.lastTouchedBy;
							}
						}
					}
				}

				// only let balls go in pocket if game isn't over
				bool collides = false;
				if (!gameOver) {
					for (Collidable& goal : goals) {
						if (ball->contains(goal)) {
							collides = true;
							if (ball->name.at(5) == '8') {
								gameOver = true;

								std::cout << "8 ball hit in - game is over." << std::endl;
								if (p1.score > p2.score) {
									std::cout << "Player 1 (stripes) wins!" << std::endl;
								} else if (p1.score < p2.score) {
									std::cout << "Player 2 (solids) wins!" << std::endl;
								} else {
									std::cout << "Player 1 and Player 2 tied!" << std::endl;
								}
								std::cout << p1.score << " vs. " << p2.score << std::endl;
								break;
							}

							if (ball->lastTouchedBy) {
								if (ball->lastTouchedBy == &p1) {
									// stripes are Ball-9, Ball-10, ..., Ball-15
									if (ball->name.at(5) == '9' || ball->name.length() == 7) {
										p1.score += 1;
										std::cout << "Player 1 earned a point" << std::endl;
									} else {
										p1.score -= 1;
										std::cout << "Player 1 lost a point" << std::endl;
									}
								} else if (ball->lastTouchedBy == &p2) {
									// solids are Ball-1, Ball-2, ..., Ball-7
									if (ball->name.at(5) < '8') {
										p2.score += 1;
										std::cout << "Player 2 earned a point" << std::endl;
									} else {
										p2.score -= 1;
										std::cout << "Player 2 lost a point" << std::endl;
									}
								}
							}
							break;
						}
					}
				} else {
					if (glm::length(ball->vel) < 2.0f) {
						ball->vel *= 1.5f;
					}
				}
				if (collides) {
					ball->obj->transform.position = glm::vec3(1.0f, 1.0f, -1.0f);
					ball = balls.erase(ball);
				} else {
					ball++;
				}
			}

			glm::vec3 p1world = p1.obj->transform.make_local_to_world() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
			glm::vec3 p2world = p2.obj->transform.make_local_to_world() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

			camera.radius = glm::length(p2world - p1world) + 0.5f;
			if (gameOver) {
				camera.radius = 4.0f;
			}

			if (camera.radius < 2.4f) {
				camera.radius = 2.4f;
			}

			camera.target = (p2world + p1world) / 2.0f;
			camera.azimuth = 0.0f;

			// camera:
			scene.camera.transform.position =
					camera.radius * glm::vec3(std::cos(camera.elevation) * std::cos(camera.azimuth),
																		std::cos(camera.elevation) * std::sin(camera.azimuth), std::sin(camera.elevation)) +
					camera.target;

			glm::vec3 out = -glm::normalize(camera.target - scene.camera.transform.position);
			glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
			up = glm::normalize(up - glm::dot(up, out) * out);
			glm::vec3 right = glm::cross(up, out);

			scene.camera.transform.rotation = glm::quat_cast(glm::mat3(right, up, out));
			scene.camera.transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
		}

		std::memcpy(prev, keys, sizeof prev);

		// draw output:
		glClearColor(0.5, 0.5, 0.5, 0.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		{	// draw game state:
			glUseProgram(program);
			glUniform3fv(program_to_light, 1, glm::value_ptr(glm::normalize(glm::vec3(0.0f, 1.0f, 10.0f))));
			scene.render();
		}

		SDL_GL_SwapWindow(window);
	}

	//------------  teardown ------------

	SDL_GL_DeleteContext(context);
	context = 0;

	SDL_DestroyWindow(window);
	window = NULL;

	return 0;
}

static GLuint compile_shader(GLenum type, std::string const& source) {
	GLuint shader = glCreateShader(type);
	GLchar const* str = source.c_str();
	GLint length = source.size();
	glShaderSource(shader, 1, &str, &length);
	glCompileShader(shader);
	GLint compile_status = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
	if (compile_status != GL_TRUE) {
		std::cerr << "Failed to compile shader." << std::endl;
		GLint info_log_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector<GLchar> info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetShaderInfoLog(shader, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		glDeleteShader(shader);
		throw std::runtime_error("Failed to compile shader.");
	}
	return shader;
}

static GLuint link_program(GLuint fragment_shader, GLuint vertex_shader) {
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);
	GLint link_status = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &link_status);
	if (link_status != GL_TRUE) {
		std::cerr << "Failed to link shader program." << std::endl;
		GLint info_log_length = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector<GLchar> info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetProgramInfoLog(program, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		throw std::runtime_error("Failed to link program");
	}
	return program;
}
