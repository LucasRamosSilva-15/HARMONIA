add_executable(harmonia_link_receiver
    harmonia_link_receiver.cpp
)

target_link_libraries(harmonia_link_receiver
    PRIVATE
        harmonia::core
)

harmonia_set_warnings(harmonia_link_receiver)

if(HARMONIA_ENABLE_SANITIZERS)
    harmonia_enable_sanitizers(harmonia_link_receiver)
endif()