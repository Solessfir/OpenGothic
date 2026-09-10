#pragma once

namespace Feedback {

enum class Effect { Navigate, Confirm, Reject, Hit, Damage };

void setEnabled(bool enabled);
void setGamepad(bool gamepad);
void play(Effect effect);
void gameplay(Effect effect);

}
