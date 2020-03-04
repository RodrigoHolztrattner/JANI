////////////////////////////////////////////////////////////////////////////////
// Filename: main.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniConfig.h"
#include "DummyWorker.h"

int main(int _argc, char* _argv[])
{
    const char*   runtime_ip          = _argv[1];
    uint32_t      runtime_listen_port = std::stoi(std::string(_argv[2]));
    Jani::LayerId layer_id          = std::stoull(std::string(_argv[3]));

    std::cout << "Worker -> Worker spawned for runtime_ip{" << runtime_ip << "}, runtime_listen_port{" << runtime_listen_port << "}, layer_id{" << layer_id << "}" << std::endl;

    bool is_brain = 1 == layer_id;
    entityx::EntityX s_ecs_manager;

    std::cout << "Worker -> " << (is_brain ? "is brain" : "is game") << std::endl;

    struct PositionComponent
    {
        int32_t x = 0;
        int32_t y = 0;
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
        std::cout << "Worker -> Unable to initialize worker!" << std::endl;
    }

    worker.RequestAuthentication().OnResponse(
        [](const Jani::Message::RuntimeAuthenticationResponse& _response, bool _timeout)
        {
            if (!_response.succeed)
            {
                std::cout << "Worker -> Failed to authenticate!" << std::endl;
            }
            else
            {
                std::cout << "Worker -> Authentication succeeded! uses_spatial_area{" << _response.use_spatial_area << "}, maximum_entities{" << _response.maximum_entity_limit << "}" << std::endl;
            }
        }).WaitResponse();

    if(is_brain)
    {
        worker.RequestReserveEntityIdRange(100).OnResponse(
            [&](const Jani::Message::RuntimeReserveEntityIdRangeResponse& _response, bool _timeout)
            {
                if (_response.succeed)
                {
                    initial_entity_id = _response.id_begin;
                    final_entity_id   = _response.id_end;
                }
            }).WaitResponse();
    }

    worker.RegisterOnComponentAddedCallback(
        [](entityx::Entity& _entity, Jani::ComponentId _component_id, const Jani::ComponentPayload& _component_payload)
        {
            // Somehow I know that component id 0 translate to Position component
            switch (_component_id)
            {
                case 0:
                {
                    std::cout << "Worker -> Assigning Position component to entity id{" << _entity << "}" << std::endl;

                    assert(!_entity.has_component<PositionComponent>());
                    _entity.assign<PositionComponent>(_component_payload.GetPayload<PositionComponent>());

                    break;
                }
                case 1:
                {
                    std::cout << "Worker -> Assigning NPC component to entity id{" << _entity << "}" << std::endl;

                    assert(!_entity.has_component<NpcComponent>());
                    _entity.assign<NpcComponent>(_component_payload.GetPayload<NpcComponent>());

                    break;
                }
                default:
                {
                    std::cout << "Worker -> Trying to assign invalid component to entity" << std::endl;

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
                    std::cout << "Worker -> Removing Position component from entity id{" << _entity << "}" << std::endl;

                    assert(_entity.has_component<PositionComponent>());
                    _entity.remove<PositionComponent>();

                    break;
                }
                case 1:
                {
                    std::cout << "Worker -> Removing NPC component from entity id{" << _entity << "}" << std::endl;

                    assert(_entity.has_component<NpcComponent>());
                    _entity.remove<NpcComponent>();

                    break;
                }
                default:
                {
                    std::cout << "Worker -> Trying to remove invalid component from entity" << std::endl;

                    break;
                }
            }
        });

    std::cout << "Worker -> Entering main loop!" << std::endl;

    while (worker.IsConnected())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        if (is_brain)
        {
            if (total_npcs_alive < maximum_npcs && rand() % 100 == 1 && initial_entity_id < final_entity_id)
            {
                auto new_entity = s_ecs_manager.entities.create();

                PositionComponent position_component = PositionComponent{ 0, 0 };

                new_entity.assign<PositionComponent>(position_component);

                std::cout << "Worker -> Requesting to add new entity!" << std::endl;

                Jani::EntityPayload entity_payload;
                entity_payload.component_payloads.push_back({ initial_entity_id , 0, {} });
                entity_payload.component_payloads.back().SetPayload(position_component);

                worker.RequestAddEntity(initial_entity_id++, std::move(entity_payload)).OnResponse(
                    [&](const Jani::Message::RuntimeDefaultResponse& _response, bool _timeout)
                    {
                        if (!_response.succeed)
                        {
                            std::cout << "Worker -> Problem adding new entity!" << std::endl;
                            initial_entity_id--;
                        }
                        else
                        {
                            std::cout << "Worker -> New entity added successfully!" << std::endl;
                        }
                    }).WaitResponse();

                total_npcs_alive++;
            }
        }
        else
        {
            worker.GetEntityManager().each<PositionComponent>(
                [&](auto entity, PositionComponent& _position)
                {
                    if (rand() % 10 == 1)
                    {
                        _position.x += (rand() % 3) - 1;
                        _position.y += (rand() % 3) - 1;
                    }

                    auto jani_entity = worker.GetJaniEntityId(entity);
                    if (jani_entity)
                    {
                        worker.ReportEntityPosition(jani_entity.value(), Jani::WorldPosition({ _position.x, _position.y }));

                        Jani::ComponentPayload component_payload;
                        component_payload.entity_owner = jani_entity.value();
                        component_payload.component_id = 0;
                        component_payload.SetPayload(_position);
                        worker.RequestUpdateComponent(jani_entity.value(), 0, component_payload, Jani::WorldPosition({ _position .x, _position .y}));
                    }
                });
        }

        worker.Update(20);
    }

    std::cout << "Worker -> Disconnected from the server!" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    return 0;
}