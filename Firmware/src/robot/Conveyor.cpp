#include "Conveyor.h"

#include "AxisDefns.h"
#include "GCode.h"
//#include "Kernel.h"
#include "Block.h"
#include "Planner.h"
#include "ConfigReader.h"
#include "StepTicker.h"
#include "Robot.h"
#include "StepperMotor.h"
#include "PlannerQueue.h"

#include <functional>
#include <vector>

#define queue_delay_time_ms_key "queue_delay_time_ms"

/*
 * The conveyor manages the planner queue, and starting the executing chain of blocks
 * TODO is this even required anymore?
 */
Conveyor *Conveyor::instance;

Conveyor::Conveyor() : Module("conveyor")
{
    instance= this;
    running = false;
    allow_fetch = false;
    flush= false;
    halted= false;
}

bool Conveyor::configure(ConfigReader& cr)
{
    // Attach to the end_of_move stepper event
    ConfigReader::section_map_t m;
    if(cr.get_section("conveyor", m)) {
        queue_delay_time_ms = cr.get_int(m, queue_delay_time_ms_key, 100);
    }
    return true;
}

// called when everything has been configured
void Conveyor::start(uint8_t n)
{
    //StepTicker.getInstance()->finished_fnc = std::bind( &Conveyor::all_moves_finished, this);
    Block::init(n); // set the number of motors which determines how big the tick info vector is
    running = true;
    pqueue= Planner::getInstance()->queue;
}

void Conveyor::on_halt(bool flg)
{
    halted= flg;

    if(flg) {
        flush_queue();
    }
}

// TODO this maybe needs to be a thread
// void Conveyor::on_idle(void*)
// {
//     if (running) {
//         check_queue();
//     }
// }

// see if we are idle
// this checks the block queue is empty, and that the step queue is empty and
// checks that all motors are no longer moving
bool Conveyor::is_idle() const
{
    if(pqueue->empty()) {
        for(auto &a : Robot::getInstance()->actuators) {
            if(a->is_moving()) return false;
        }
        return true;
    }

    return false;
}

// Wait for the queue to be empty and for all the jobs to finish in step ticker
// This must be called in the command thread context and will stall the command thread
void Conveyor::wait_for_idle(bool wait_for_motors)
{
    // wait for the job queue to empty, this means cycling everything on the block queue into the job queue
    // forcing them to be jobs
    running = false; // stops on_idle calling check_queue
    while (!pqueue->empty()) {
        check_queue(true); // forces queue to be made available to stepticker
        // THEKERNEL->call_event(ON_IDLE, this);
    }

    if(wait_for_motors) {
        // now we wait for all motors to stop moving
        while(!is_idle()) {
            // THEKERNEL->call_event(ON_IDLE, this);
        }
    }

    running = true;
    // returning now means that everything has totally finished
}

// TODO
//     // not sure if this is the correct place but we need to turn on the motors if they were not already on
//     THEKERNEL->call_event(ON_ENABLE, (void*)1); // turn all enable pins on
//

// TODO
// should be called when idle, but we haev no on_idle so where is this called from? do we have thread?
void Conveyor::check_queue(bool force)
{
    static systime_t last_time_check = clock_systimer();
    if(pqueue->empty()) {
        allow_fetch = false;
        last_time_check = clock_systimer(); // reset timeout
        return;
    }

    // if we have been waiting for more than the required waiting time and the queue is not empty, or the queue is full, then allow stepticker to get the tail
    // we do this to allow an idle system to pre load the queue a bit so the first few blocks run smoothly.
    if(force || pqueue->full() || (TICK2USEC(clock_systimer() - last_time_check) >= (queue_delay_time_ms * 1000)) ) {
        last_time_check = clock_systimer(); // reset timeout
        if(!flush) allow_fetch = true;
        return;
    }
}

// called from step ticker ISR
// we only ever access or change the read/tail index of the queue so this is thread safe
bool Conveyor::get_next_block(Block **block)
{
    // empty the entire queue
    if (flush){
        while (!pqueue->empty()) {
            pqueue->release_tail();
        }
    }

    // default the feerate to zero if there is no block available
    this->current_feedrate= 0;

    if(halted || pqueue->empty()) return false; // we do not have anything to give

    // wait for queue to fill up, optimizes planning
    if(!allow_fetch) return false;

    Block *b= pqueue->get_tail();
    // we cannot use this now if it is being updated
    if(!b->locked) {
        assert(b->is_ready); // should never happen

        b->is_ticking= true;
        b->recalculate_flag= false;
        this->current_feedrate= b->nominal_speed;
        *block= b;
        return true;
    }

    return false;
}

// called from step ticker ISR when block is finished, do not do anything slow here
void Conveyor::block_finished()
{
    // release the tail
    pqueue->release_tail();
}

/*
    In most cases this will not totally flush the queue, as when streaming
    gcode there is one stalled waiting for space in the queue, in
    queue_head_block() so after this flush, once main_loop runs again one more
    gcode gets stuck in the queue, this is bad. Current work around is to call
    this when the queue in not full and streaming has stopped
*/
void Conveyor::flush_queue()
{
    allow_fetch = false;
    flush= true;

    // TODO force deceleration of last block

    // now wait until the block queue has been flushed
    wait_for_idle(false);

    flush= false;
}

// Debug function
// Probably not thread safe
// only call within command thread context or not while planner is running
void Conveyor::dump_queue()
{
    // start the iteration at the head
    pqueue->start_iteration();
    Block *b = pqueue->get_head();
    int i= 0;
    while (!pqueue->is_at_tail()) {
        printf("block %03d > ", ++i);
        b->debug();
        b= pqueue->tailward_get(); // walk towards the tail
    }
}