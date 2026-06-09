#include "game/ereg.h"

#include "game/gconfig.h"

// Original implementation spawned the Interplay electronic-registration
// helper. We just bump the run counter.
void annoy_user()
{
    int timesRun = 0;
    config_get_value(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_TIMES_RUN_KEY, &timesRun);
    config_set_value(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_TIMES_RUN_KEY, timesRun + 1);
}
