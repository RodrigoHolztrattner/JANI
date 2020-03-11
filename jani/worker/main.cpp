////////////////////////////////////////////////////////////////////////////////
// Filename: main.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniConfig.h"
#include "DummyWorker.h"

int main(int _argc, char* _argv[])
{
    Jani::InitializeStandardConsole();

    const char*   runtime_ip          = _argv[1];
    uint32_t      runtime_listen_port = std::stoi(std::string(_argv[2]));
    Jani::LayerId layer_id            = std::stoull(std::string(_argv[3]));

    Jani::MessageLog().Info("Worker -> Worker spawned for runtime_ip {}, runtime_listen_port {}, layer_id {}", runtime_ip, runtime_listen_port, layer_id);

    bool is_brain = 1 == layer_id;
    entityx::EntityX s_ecs_manager;

    Jani::MessageLog().Info("Worker -> {}", (is_brain ? "is brain" : "is game"));

    struct PositionComponent
    {
        glm::vec2 position;
        glm::vec2 target_position;

        float speed = 1.0f;

        float time_to_move = 0.0f;

        bool is_moving = false;
    };

    struct NpcComponent
    {
        bool alive = true;
    };

    uint32_t total_npcs_alive = 0;
    uint32_t maximum_npcs     = 40;

    Jani::EntityId initial_entity_id = 0;
    Jani::EntityId final_entity_id   = 0;

    DummyWorker worker(layer_id);

    if (!worker.InitializeWorker(runtime_ip, runtime_listen_port))
    {
        Jani::MessageLog().Critical("Worker -> Unable to initialize worker");
    }

    worker.RegisterOnComponentUpdateCallback(
        [](entityx::Entity& _entity, Jani::ComponentId _component_id, const Jani::ComponentPayload& _component_payload)
        {
            // Somehow I know that component id 0 translate to Position component
            switch (_component_id)
            {
                case 0:
                {
                    _entity.replace<PositionComponent>(_component_payload.GetPayload<PositionComponent>());

                    break;
                }
                case 1:
                {
                    _entity.replace<NpcComponent>(_component_payload.GetPayload<NpcComponent>());

                    break;
                }
                default:
                {
                    Jani::MessageLog().Error("Worker -> Trying to assign invalid component to entity");

                    break;
                }
            }
        });

    worker.RegisterOnComponentRemovedCallback(
        [](entityx::Entity& _entity, Jani::ComponentId _component_id)
        {
            // Somehow I know that component id 0 translate to Position component
            switch (_component_id)
            {
                case 0:
                {
                    assert(_entity.has_component<PositionComponent>());
                    _entity.remove<PositionComponent>();

                    break;
                }
                case 1:
                {
                    assert(_entity.has_component<NpcComponent>());
                    _entity.remove<NpcComponent>();

                    break;
                }
                default:
                {
                    Jani::MessageLog().Error("Worker -> Trying to remove invalid component from entity");

                    break;
                }
            }
        });

    worker.RegisterOnAuthorityGainCallback(
        [&](entityx::Entity& _entity)
        {
            if (is_brain)
            {
                return;
            }

            Jani::ComponentQuery component_query;
            component_query.Begin(0)
                .QueryComponent(0)
                .InRadius(30)
                .WithFrequency(1);

            worker.RequestUpdateComponentInterestQuery(
                worker.GetJaniEntityId(_entity).value(),
                0,
                { std::move(component_query) });
        });

    worker.RegisterOnAuthorityLostCallback(
        [](entityx::Entity& _entity)
        {

        });

    bool authenticated = false;

    worker.RequestAuthentication().OnResponse(
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

    if(is_brain)
    {
        worker.RequestReserveEntityIdRange(5001).OnResponse(
            [&](const Jani::Message::RuntimeReserveEntityIdRangeResponse& _response, bool _timeout)
            {
                if (_response.succeed)
                {
                    initial_entity_id = _response.id_begin;
                    final_entity_id   = _response.id_end;
                }
            }).WaitResponse();
    }

    Jani::MessageLog().Info("Worker -> Entering main loop");

    while (worker.IsConnected() && authenticated)
    {
        uint32_t wait_time_ms = 20;
        float    time_elapsed = static_cast<float>(wait_time_ms) / 1000.0f;

        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time_ms));

        if (is_brain)
        {
            if (total_npcs_alive < maximum_npcs && rand() % 2 == 1 && initial_entity_id < final_entity_id)
            {
                auto new_entity = s_ecs_manager.entities.create();

                PositionComponent position_component;
                position_component.position        = glm::vec2(0.0f, 0.0f);
                position_component.target_position = glm::vec2(0.0f, 0.0f);
                position_component.speed           = (rand() % 1900 + 100) / 100.0f;
                position_component.time_to_move    = 0.0f;
                position_component.is_moving       = false;

                new_entity.assign<PositionComponent>(position_component);

                Jani::MessageLog().Info("Worker -> Requesting to add new entity");

                Jani::EntityPayload entity_payload;
                entity_payload.component_payloads.push_back({ initial_entity_id , 0, {} });
                entity_payload.component_payloads.back().SetPayload(position_component);

                worker.RequestAddEntity(initial_entity_id++, std::move(entity_payload)).OnResponse(
                    [&](const Jani::Message::RuntimeDefaultResponse& _response, bool _timeout)
                    {
                        if (!_response.succeed)
                        {
                            Jani::MessageLog().Error("Worker -> Problem adding new entity");
                            initial_entity_id--;
                        }
                        else
                        {
                            Jani::MessageLog().Info("Worker -> New entity added successfully");
                        }
                    }).WaitResponse();

                total_npcs_alive++;
            }
        }
        else
        {
            std::unordered_set<entityx::Entity> updated_x_entities;
            std::unordered_set<Jani::EntityId> updated_entities;
            worker.GetEntityManager().each<PositionComponent>(
                [&](auto entity, PositionComponent& _position)
                {
                    assert(updated_x_entities.find(entity) == updated_x_entities.end());
                    updated_x_entities.insert(entity);

                    auto RandomFloat = [](float _from, float _to) -> float
                    {
                        float random = ((float)rand()) / (float)RAND_MAX;
                        float diff   = _to - _from;
                        float r      = random * diff;
                        return _from + r;
                    };

                    auto RandomVec2 = [&](float _from, float _to) -> glm::vec2
                    {
                        return glm::vec2(RandomFloat(_from, _to), RandomFloat(_from, _to));
                    };

                    float world_bounds      = 100.0f;
                    _position.time_to_move -= time_elapsed;
                    if (_position.time_to_move < 0 && !_position.is_moving)
                    {
                        _position.is_moving       = true;
                        _position.target_position = RandomVec2(-world_bounds, world_bounds);
                    }

                    if (_position.is_moving)
                    {
                        _position.position -= glm::normalize(_position.position - _position.target_position) * _position.speed * time_elapsed;
                        if (glm::distance(_position.position, _position.target_position) < 10.0f)
                        {
                            _position.is_moving    = false;
                            _position.time_to_move = RandomFloat(10.0f, 1.0f);
                        }
                    }

                    auto jani_entity = worker.GetJaniEntityId(entity);
                    if (jani_entity)
                    {
                        assert(updated_entities.find(jani_entity.value()) == updated_entities.end());
                        updated_entities.insert(jani_entity.value());

                        Jani::ComponentPayload component_payload;
                        component_payload.entity_owner = jani_entity.value();
                        component_payload.component_id = 0;
                        component_payload.SetPayload(_position);
                        worker.RequestUpdateComponent(jani_entity.value(), 0, component_payload, Jani::WorldPosition({ static_cast<int32_t>(_position.position.x), static_cast<int32_t>(_position.position.y) }));
                    }
                });
        }

        worker.Update(wait_time_ms);
    }

    Jani::MessageLog().Warning("Worker -> Disconnected from the server");

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    return 0;
}