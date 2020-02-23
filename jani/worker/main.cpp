////////////////////////////////////////////////////////////////////////////////
// Filename: main.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniConfig.h"

int main(int _argc, char* _argv[])
{
    int from;
    int to;
    bool is_server = _argc == 4;
    if (is_server)
    {
        from = 8080;
        to   = 8081;   
    }
    else
    {
        from = 8081;
        to   = 8080;
    }

    Jani::Connection connection(5, from, to, "127.0.0.1");

    while (true)
    {
        if (is_server)
        {
            std::string send_value;
            std::cout << "Send: ";
            std::cin >> send_value;
            std::cout << std::endl;

            if (send_value == "exit")
            {
                break;
            }

            connection.Send(send_value.data(), send_value.size());
            std::cout << "Sent: " << send_value << " {" << send_value.size() << "}" << std::endl;
        }
        else
        {
            char buffer[1024];
            auto total = connection.Receive(buffer, 1024);
            if (total)
            {
                std::cout << "Received: " << std::string(buffer, total.value()) << " {" << total.value() << "}" << std::endl;
            }
        }
    }

    worker::RequestId<worker::CreateEntityRequest> entity_creation_request_id;
    worker::RequestId<worker::DeleteEntityRequest> entity_deletion_request_id;

    constexpr uint32_t kTimeoutMillis = 500;

    void CreateDeleteEntity(worker::Connection & connection, worker::Dispatcher & dispatcher,
                            const improbable::EntityAcl::Data & acl) {
        // Reserve an entity ID.
        worker::RequestId<worker::ReserveEntityIdsRequest> entity_id_reservation_request_id =
            connection.SendReserveEntityIdsRequest(1, kTimeoutMillis);

        // When the reservation succeeds, create an entity with the reserved ID.
        dispatcher.OnReserveEntityIdsResponse([entity_id_reservation_request_id, &connection,
                                              acl](const worker::ReserveEntityIdsResponseOp& op) {
                                                  if (op.RequestId == entity_id_reservation_request_id &&
                                                      op.StatusCode == worker::StatusCode::kSuccess) {
                                                      // ID reservation was successful - create an entity with the reserved ID.
                                                      worker::Entity entity;
                                                      entity.Add<improbable::Position>({ {1, 2, 3} });
                                                      entity.Add<improbable::EntityAcl>(acl);
                                                      auto result = connection.SendCreateEntityRequest(entity, op.FirstEntityId, kTimeoutMillis);
                                                      // Check no errors occurred.
                                                      if (result) {
                                                          entity_creation_request_id = *result;
                                                      }
                                                      else {
                                                          connection.SendLogMessage(worker::LogLevel::kError, "CreateDeleteEntity",
                                                                                    result.GetErrorMessage());
                                                          std::terminate();
                                                      }
                                                  }
                                              });

        // When the creation succeeds, delete the entity.
        dispatcher.OnCreateEntityResponse([&connection](const worker::CreateEntityResponseOp& op) {
            if (op.RequestId == entity_creation_request_id &&
                op.StatusCode == worker::StatusCode::kSuccess) {
                entity_deletion_request_id = connection.SendDeleteEntityRequest(*op.EntityId, kTimeoutMillis);
            }
                                          });

        // When the deletion succeeds, we're done.
        dispatcher.OnDeleteEntityResponse([](const worker::DeleteEntityResponseOp& op) {
            if (op.RequestId == entity_deletion_request_id &&
                op.StatusCode == worker::StatusCode::kSuccess) {
                // Test successful!
            }
                                          });
    }

    return 0;
}