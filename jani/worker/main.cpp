////////////////////////////////////////////////////////////////////////////////
// Filename: main.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniInternal.h"
#include "worker/JaniWorker.h"
#include "worker/JaniEntityManager.h"
#include "worker/JaniClientEntity.h"

LONG WINAPI MyUnhandledExceptionFilter(PEXCEPTION_POINTERS exception)
{
    Jani::MessageLog().Critical("Worker -> Crash!");
    DebugBreak();
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI RedirectedSetUnhandledExceptionFilter(EXCEPTION_POINTERS* /*ExceptionInfo*/)
{
    // When the CRT calls SetUnhandledExceptionFilter with NULL parameter
    // our handler will not get removed.
    return 0;
}

int main(int _argc, char* _argv[])
{
    ::SetUnhandledExceptionFilter(MyUnhandledExceptionFilter);

    Jani::InitializeStandardConsole();

    const char*   runtime_ip          = _argv[1];
    uint32_t      runtime_listen_port = std::stoi(std::string(_argv[2]));
    Jani::LayerId layer_id            = std::stoull(std::string(_argv[3]));

    Jani::MessageLog().Info("Worker -> Worker spawned for runtime_ip {}, runtime_listen_port {}, layer_id {}", runtime_ip, runtime_listen_port, layer_id);

    bool is_brain = 1 == layer_id;

    Jani::MessageLog().Info("Worker -> {}", (is_brain ? "is brain" : "is game"));

    struct PositionComponent
    {
        glm::vec2 position;
        glm::vec2 target_position;

        float speed = 1.0f;

        float time_to_move = 0.0f;

        bool is_moving = false;

        Jani::WorldPosition GetEntityWorldPosition()
        {
            return Jani::WorldPosition({ static_cast<int32_t>(position.x), static_cast<int32_t>(position.y) });
        }

    };

    struct NpcComponent
    {
        bool alive = true;

    };

    uint32_t total_npcs_alive = 0;
    uint32_t maximum_npcs     = 40;

    std::unique_ptr<Jani::Worker>        worker         = std::make_unique<Jani::Worker>(layer_id);
    std::unique_ptr<Jani::EntityManager> entity_manager = std::make_unique<Jani::EntityManager>(*worker);

    entity_manager->RegisterComponent<PositionComponent>(0);
    entity_manager->RegisterComponent<NpcComponent>(1);

    if (!worker->InitializeWorker(runtime_ip, runtime_listen_port))
    {
        Jani::MessageLog().Critical("Worker -> Unable to initialize worker");
    }

    bool authenticated = false;

    worker->RequestAuthentication().OnResponse(
        [&](const Jani::Message::RuntimeAuthenticationResponse& _response, bool _timeout)
        {
            if (!_response.succeed)
            {
                Jani::MessageLog().Critical("Worker -> Failed to authenticate");
            }
            else
            {
                Jani::MessageLog().Info("Worker -> Authentication succeeded! uses_spatial_area {}, maximum_entities {}", _response.use_spatial_area, _response.maximum_entity_limit);
                authenticated = true;
            }
        }).WaitResponse();

    Jani::MessageLog().Info("Worker -> Entering main loop");

    while (worker->IsConnected() && authenticated)
    {
        uint32_t wait_time_ms = 20;
        float    time_elapsed = static_cast<float>(wait_time_ms) / 1000.0f;

        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time_ms));

        if (is_brain)
        {
            if (total_npcs_alive < maximum_npcs && rand() % 2 == 1)
            {
                PositionComponent position_component;
                position_component.position        = glm::vec2(0.0f, 0.0f);
                position_component.target_position = glm::vec2(0.0f, 0.0f);
                position_component.speed           = (rand() % 1900 + 100) / 100.0f;
                position_component.time_to_move    = 0.0f;
                position_component.is_moving       = false;

                NpcComponent npc_component;
                npc_component.alive = true;

                Jani::MessageLog().Info("Worker -> Requesting to add new entity");

                entity_manager->CreateEntity(std::move(position_component), std::move(npc_component));

                total_npcs_alive++;
            }
        }
        else
        {
            entity_manager->ForEach<PositionComponent>(
                [&](Jani::Entity entity, PositionComponent& _position)
                {
                    auto RandomFloat = [](float _from, float _to) -> float
                    {
                        float random = ((float)rand()) / (float)RAND_MAX;
                        float diff = _to - _from;
                        float r = random * diff;
                        return _from + r;
                    };

                    auto RandomVec2 = [&](float _from, float _to) -> glm::vec2
                    {
                        return glm::vec2(RandomFloat(_from, _to), RandomFloat(_from, _to));
                    };

                    float world_bounds = 100.0f;
                    _position.time_to_move -= time_elapsed;
                    if (_position.time_to_move < 0 && !_position.is_moving)
                    {
                        _position.is_moving = true;
                        _position.target_position = RandomVec2(-world_bounds, world_bounds);
                    }

                    if (_position.is_moving)
                    {
                        _position.position -= glm::normalize(_position.position - _position.target_position) * _position.speed * time_elapsed;
                        if (glm::distance(_position.position, _position.target_position) < 10.0f)
                        {
                            _position.is_moving = false;
                            _position.time_to_move = RandomFloat(10.0f, 1.0f);
                        }
                    }

                    entity_manager->AcknowledgeComponentUpdate<PositionComponent>(entity);
                });
        }

        worker->Update(wait_time_ms);
    }

    Jani::MessageLog().Warning("Worker -> Disconnected from the server");

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    return 0;
}