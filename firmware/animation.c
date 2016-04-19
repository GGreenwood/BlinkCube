#include "animation.h"

// Apply brightness to a color. Brightness is between 0 and 255
static inline Color apply_brightness(Color color, uint16_t brightness) {
    return (Color) {
        color.r * brightness / 255,
        color.g * brightness / 255,
        color.b * brightness / 255
    };
}

// Apply brightness to a color depending on the ratio of position to max
// Useful for turning a linear increase(1,2,3.../20) into a color pulse
static Color brightness_from_position(Color color, uint8_t position, uint8_t max) {
    return apply_brightness(color, ((max - position) % (max + 1)) * 255 / max);
}

static Direction get_analog_direction(Controller *controller) {
    if(ANALOG_UP(*controller)) {
        return D_UP;
    } else if(ANALOG_DOWN(*controller)) {
        return D_DOWN;
    } else if(ANALOG_LEFT(*controller)) {
        return D_LEFT;
    } else if(ANALOG_RIGHT(*controller)) {
        return D_RIGHT;
    }
    return D_NONE;
}

static Direction get_c_direction(Controller *controller) {
    if(C_UP(*controller)) {
        return D_UP;
    } else if(C_DOWN(*controller)) {
        return D_DOWN;
    } else if(C_LEFT(*controller)) {
        return D_LEFT;
    } else if(C_RIGHT(*controller)) {
        return D_RIGHT;
    }
    return D_NONE;
}

void static *reset_animation(State *state) {
    state->action = IDLE;
    state->color1 = (Color) {255, 255, 255};
    state->color2 = (Color) {0, 0, 0};
    state->dir = D_NONE;
    state->interruptable = true;
    return 0;
}


State *init_animation(void) {
    State *init;
    init = (State*)malloc(sizeof(State));
    reset_animation(init);

    return init;
}

void next_frame(State *state, Controller *controller) {
    // Update the animation state machine
    state->timer++;

    // Test if the current animation has timed out
    if(state->timer >= state->timeout) {
        reset_animation(state);
        state->action = BLANK;
    }

    // Check for wobbling
    if(CONTROLLER_A(*controller) && state->wobble_timer > 0) {
        state->wobble_timer = 0;
        state->wobble_counter++;
    } else if(!CONTROLLER_A(*controller)) {
        state->wobble_timer++;
    }
    if(state->wobble_timer >= 0xff) {
        state->wobble_counter = 0;
    }

    Direction analog_direction = get_analog_direction(controller);

    // If we're in Side-B, make sure B is released before repeating
    if((!state->interruptable) && (state->action == SIDEB) && (!CONTROLLER_B(*controller))) {
        state->interruptable = true;
    } // If the state can be interrupted, check all of the possible outcomes
    else if(state->interruptable) {
        Direction c_direction = get_c_direction(controller);
        // Check if we're wobbling
        if (state->wobble_counter >= 5) {
            state->action = PULSE;
            state->color1 = COLOR_PINK;
            state->color2 = COLOR_NONE;
            state->dir= D_NONE;
            state->timer = 0;
            state->interruptable = true;
            state->timeout = 20;
            state->pulse_length = 20;
            state->echo = false;
        } // Check for Blizzard(Down-B)
        else if(CONTROLLER_B(*controller) && analog_direction == D_DOWN) {
            state->action = BLIZZARD;
            state->color1 = COLOR_WHITE;
            state->color2 = COLOR_BLUE;
            state->dir= D_NONE;
            state->timer = 0;
            state->interruptable = false;
            state->timeout = 90;
            state->pulse_length = 12;
            state->echo = false;
        } // Check for jump(X or Y)
        else if(CONTROLLER_X(*controller) || CONTROLLER_Y(*controller)) {
            state->action = PULSE;
            state->color1 = COLOR_WHITE;
            state->color2 = COLOR_NONE;
            state->dir= D_NONE;
            state->timer = 0;
            state->interruptable = true;
            state->timeout = 20;
            state->pulse_length = 20;
            state->echo = false;
        } // Check for grab(Z, or Analog L/R and A)
        else if(CONTROLLER_Z(*controller) || (CONTROLLER_A(*controller) && 
                (ANALOG_L(*controller) || ANALOG_R(*controller)))) {
            state->action = PULSE;
            state->color1 = COLOR_PURPLE;
            state->color2 = COLOR_NONE;
            state->dir= D_NONE;
            state->timer = 0;
            state->interruptable = true;
            state->timeout = 20;
            state->pulse_length = 20;
            state->echo = false;
        } // Check for Ice blocks(Neutral B)
        else if(CONTROLLER_B(*controller) && analog_direction == D_NONE) {
            state->action = PULSE;
            state->color1 = COLOR_LIGHT_BLUE;
            state->color2 = COLOR_NONE;
            state->dir= D_NONE;
            state->timer = 0;
            state->interruptable = true;
            state->timeout = 20;
            state->pulse_length = 20;
            state->echo = false;
        } // Check for aerials, smashes, or tilts
        else if((CONTROLLER_A(*controller) && analog_direction != D_NONE) ||
                (c_direction != D_NONE)) {
            state->action = PULSE;
            state->color1 = COLOR_LIGHT_BLUE;
            state->color2 = COLOR_PINK;
            if(analog_direction != D_NONE) {
                state->dir= analog_direction;
            } else {
                state->dir= c_direction;
            }
            state->timer = 0;
            state->interruptable = true;
            state->timeout = 40;
            state->pulse_length = 20;
            state->echo = true;
        } // Check for Side-B
        else if(CONTROLLER_B(*controller) && (analog_direction == D_LEFT || analog_direction == D_RIGHT)){
            // Check if this is the first Side-B
            if(state->action != SIDEB) {
                state->action = SIDEB;
                state->color1 = (Color) {30, 0, 0};
                state->color2 = COLOR_NONE;
                state->dir= D_DOWN;
                state->timer = 0;
                state->interruptable = false;
                state->timeout = 40;
                state->pulse_length = 10;
                state->echo = false;
            } else {
                state->timer = 0;
                state->color1.r = min(0xff, state->color1.r + 8);
            }
        }
    }


    // Push the animations to the LEDs
    if(state->action == BLIZZARD)
    {
        // Cut into two cycles: 0<->length, length+1<->length*2
        uint8_t position = state->timer % (state->pulse_length * 2);
        // First cycle, 0<->length
        if(position < state->pulse_length) {
            Color color1 = brightness_from_position(state->color1, position, state->pulse_length);
            Color color2 = brightness_from_position(state->color2, position, state->pulse_length);
            sendPixel(color1);
            sendPixel(color2);
            sendPixel(color1);
            sendPixel(color2);
            sendPixel(color1);
            show();
        } else { // Second cycle, length+1<->length*2
            // Subtract the length to fix the offset
            position = position - state->pulse_length;

            Color color1 = brightness_from_position(state->color1, position, state->pulse_length);
            Color color2 = brightness_from_position(state->color2, position, state->pulse_length);
            sendPixel(color2);
            sendPixel(color1);
            sendPixel(color2);
            sendPixel(color1);
            sendPixel(color2);
            show();
        }
    } else if(state->action == PULSE) {
        uint8_t position = state->timer;
        //uint8_t position = 0;

        Color colors[5];
        for(uint8_t i = 0; i < 5; i++) {
            if((position >= (PULSE_DELAY * i)) && (position < state->pulse_length + (PULSE_DELAY * i))) {
                colors[i] = brightness_from_position(state->color1, position - (PULSE_DELAY * i), state->pulse_length);
            } else if((state->echo) &&
                    (position > (PULSE_DELAY * (i+1))) &&
                    (position < state->pulse_length + (PULSE_DELAY * (i + 1)))) {
                colors[i] = brightness_from_position(state->color2, position - (PULSE_DELAY * (i + 1)), state->pulse_length);
            } else {
                colors[i] = COLOR_NONE;
            }
        }

        switch(state->dir) {
            case(D_NONE):
                showColor(colors[0]);
                break;
            case(D_UP):
                sendPixel(colors[0]);
                sendPixel(colors[1]);
                sendPixel(colors[2]);
                sendPixel(colors[1]);
                sendPixel(colors[0]);
                break;
            case(D_DOWN):
                sendPixel(colors[2]);
                sendPixel(colors[1]);
                sendPixel(colors[0]);
                sendPixel(colors[1]);
                sendPixel(colors[2]);
                show();
                break;
            case(D_LEFT):
                sendPixel(colors[4]);
                sendPixel(colors[3]);
                sendPixel(colors[2]);
                sendPixel(colors[1]);
                sendPixel(colors[0]);
                show();
                break;
            case(D_RIGHT):
                sendPixel(colors[0]);
                sendPixel(colors[1]);
                sendPixel(colors[2]);
                sendPixel(colors[3]);
                sendPixel(colors[4]);
                show();
                break;
        }
    } else if(state->action == SIDEB) {
        uint8_t position = state->timer;

        Color color1;
        if((position >= PULSE_DELAY) && (position < state->pulse_length + PULSE_DELAY)) {
            color1 = brightness_from_position(state->color1, position - PULSE_DELAY , state->pulse_length);
        } else {
            color1 = COLOR_NONE;
        }
        showColor(color1);
    } else if(state->action == WOBBLE) {
    } else if(state->action == IDLE) {
    } else if(state->action == BLANK) {
        showColor(COLOR_NONE);
    }
}
