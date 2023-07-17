include(FetchContent)

FetchContent_Declare(perfstubs
                     GIT_REPOSITORY https://github.com/UO-OACISS/perfstubs.git
                     GIT_TAG master)
FetchContent_MakeAvailable(perfstubs)