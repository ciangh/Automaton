/*
  Automaton.cpp - Library for creating and running Finite State Machines.
  Published under the MIT License (MIT), Copyright (c) 2015, J.P. van der Landen
*/

#include "Automaton.h"

bool atm_connector::push( Factory* f /* = 0 */, bool noCallback /* = false */ ) {
  switch ( mode_flags & B00000111 ) {
    case MODE_CALLBACK:
      if ( noCallback ) {
        return false;
      } else {
        ( *callback )( callback_idx );
      }
      return true;
    case MODE_MACHINE:
      machine->trigger( event );
      return true;
    case MODE_TMACHINE:
      tmachine->trigger( event );
      return true;
    case MODE_FACTORY:
      if ( f ) f->trigger( label, event );
      return true;
  }
  return true;
}

int atm_connector::pull( Factory* f /* = 0 */, bool def_value /* = false */ ) {
  switch ( mode_flags & B00000111 ) {
    case MODE_CALLBACK:
      return ( *callback )( callback_idx );
    case MODE_MACHINE:
      return machine->state();
    case MODE_TMACHINE:
      return tmachine->state();
    case MODE_FACTORY:
      if ( f ) return f->state( label );
  }
  return def_value;
}

int8_t atm_connector::logOp( void ) {
  return ( mode_flags & B00011000 ) >> 3;
}

int8_t atm_connector::relOp( void ) {
  return ( mode_flags & B11100000 ) >> 5;
}

void atm_connector::set( atm_cb_t cb, int idx, int8_t logOp /* = 0 */, int8_t relOp /* = 0 */ ) {
  mode_flags = MODE_CALLBACK | ( logOp << 3 ) | ( relOp << 5 );
  callback = cb;
  callback_idx = idx;
}

void atm_connector::set( Machine* m, int evt, int8_t logOp /* = 0 */, int8_t relOp /* = 0 */ ) {
  mode_flags = MODE_MACHINE | ( logOp << 3 ) | ( relOp << 5 );
  machine = m;
  event = evt;
}

void atm_connector::set( TinyMachine* tm, int evt, int8_t logOp /* = 0 */, int8_t relOp /* = 0 */ ) {
  mode_flags = MODE_TMACHINE | ( logOp << 3 ) | ( relOp << 5 );
  tmachine = tm;
  event = evt;
}

void atm_connector::set( const char* l, int evt, int8_t logOp /* = 0 */, int8_t relOp /* = 0 */ ) {
  mode_flags = MODE_FACTORY | ( logOp << 3 ) | ( relOp << 5 );
  label = l;
  event = evt;
}

int8_t atm_connector::mode( void ) {
  return mode_flags & B00000111;
}

void atm_timer_millis::set( uint32_t v ) {
  value = v;
}

void atm_timer_micros::set( uint32_t v ) {
  value = v;
}

int atm_timer_millis::expired( BaseMachine* machine ) {
  return value == ATM_TIMER_OFF ? 0 : millis() - machine->state_millis >= value;
}

int atm_timer_micros::expired( Machine* machine ) {
  return value == ATM_TIMER_OFF ? 0 : micros() - machine->state_micros >= value;
}

void atm_counter::set( uint16_t v ) {
  value = v;
}

uint16_t atm_counter::decrement( void ) {
  return value > 0 && value != ATM_COUNTER_OFF ? --value : 0;
}

uint8_t atm_counter::expired() {
  return value == ATM_COUNTER_OFF ? 0 : ( value > 0 ? 0 : 1 );
}

uint8_t atm_pin::change( uint8_t pin ) {
  unsigned char v = digitalRead( pin ) ? 1 : 0;
  if ( ( ( pinstate >> pin ) & 1 ) != ( v == 1 ) ) {
    pinstate ^= ( (uint32_t)1 << pin );
    return 1;
  }
  return 0;
}

Machine& Machine::state( state_t state ) {
  next = state;
  last_trigger = -1;
  flags &= ~ATM_SLEEP_FLAG;
  return *this;
}

int Machine::state() {
  return current;
}

Machine& Machine::trigger( int evt /* = 0 */ ) {
  state_t new_state;
  int max_cycle = 8;
  do {
    flags &= ~ATM_SLEEP_FLAG;
    cycle();
    new_state = read_state( state_table + ( current * state_width ) + evt + ATM_ON_EXIT + 1 );
  } while ( --max_cycle && ( new_state == -1 || next_trigger != -1 ) );
  if ( new_state > -1 ) {
    next_trigger = evt;
    flags &= ~ATM_SLEEP_FLAG;
    cycle();  // Pick up the trigger
    flags &= ~ATM_SLEEP_FLAG;
    cycle();  // Process the state change
  }
  return *this;
}

Machine& Machine::setTrace( Stream* stream, swcb_sym_t callback, const char symbols[] ) {
  callback_trace = callback;
  stream_trace = stream;
  _symbols = symbols;
  return *this;
}

Machine& Machine::label( const char label[] ) {
  inst_label = label;
  return *this;
}

int8_t Machine::priority() {
  return prio;
}

Machine& Machine::priority( int8_t priority ) {
  prio = priority;
  return *this;
}

uint8_t BaseMachine::sleep( int8_t v /* = 1 */ ) {
  if ( v > -1 ) flags = v ? flags | ATM_SLEEP_FLAG : flags & ~ATM_SLEEP_FLAG;
  return ( flags & ATM_SLEEP_FLAG ) > 0;
}

Machine& Machine::begin( const state_t* tbl, int width ) {
  state_table = tbl;
  state_width = ATM_ON_EXIT + width + 2;
  prio = ATM_DEFAULT_PRIO;
  if ( !inst_label ) inst_label = class_label;
  flags &= ~ATM_SLEEP_FLAG;
  return *this;
}

const char* Machine::mapSymbol( int id, const char map[] ) {
  int cnt = 0;
  int i = 0;
  if ( id == -1 ) return "*NONE*";
  if ( id == 0 ) return map;
  while ( 1 ) {
    if ( map[i] == '\0' && ++cnt == id ) {
      i++;
      break;
    }
    i++;
  }
  return &map[i];
}

// .cycle() Executes one cycle of a state machine
Machine& Machine::cycle( uint32_t time /* = 0 */ ) {
  uint32_t cycle_start = millis();
  do {
    if ( ( flags & ( ATM_SLEEP_FLAG | ATM_CYCLE_FLAG ) ) == 0 ) {
      cycles++;
      flags |= ATM_CYCLE_FLAG;
      if ( next != -1 ) {
        action( ATM_ON_SWITCH );
        if ( callback_trace ) {
          callback_trace( stream_trace, inst_label, mapSymbol( current == -1 ? current : current + state_width - ATM_ON_EXIT - 1, _symbols ),
                          mapSymbol( next == -1 ? next : next + state_width - ATM_ON_EXIT - 1, _symbols ), mapSymbol( last_trigger, _symbols ),
                          millis() - state_millis, cycles );
        }
        if ( current > -1 ) action( read_state( state_table + ( current * state_width ) + ATM_ON_EXIT ) );
        current = next;
        next = -1;
        state_millis = millis();
        state_micros = micros();
        action( read_state( state_table + ( current * state_width ) + ATM_ON_ENTER ) );
        if ( read_state( state_table + ( current * state_width ) + ATM_ON_LOOP ) == ATM_SLEEP ) {
          flags |= ATM_SLEEP_FLAG;
        } else {
          flags &= ~ATM_SLEEP_FLAG;
        }
        cycles = 0;
      }
      state_t i = read_state( state_table + ( current * state_width ) + ATM_ON_LOOP );
      if ( i != -1 ) {
        action( i );
      }
      for ( i = ATM_ON_EXIT + 1; i < state_width; i++ ) {
        state_t next_state = read_state( state_table + ( current * state_width ) + i );
        if ( ( next_state != -1 ) && ( i == state_width - 1 || event( i - ATM_ON_EXIT - 1 ) || next_trigger == i - ATM_ON_EXIT - 1 ) ) {
          state( next_state );
          last_trigger = i - ATM_ON_EXIT - 1;
          next_trigger = -1;
          break;
        }
      }
      flags &= ~ATM_CYCLE_FLAG;
    }
  } while ( millis() - cycle_start < time );
  return *this;
}

// TINY MACHINE

TinyMachine& TinyMachine::state( tiny_state_t state ) {
  next = state;
  flags &= ~ATM_SLEEP_FLAG;
  return *this;
}

int TinyMachine::state() {
  return current;
}

TinyMachine& TinyMachine::trigger( int evt /* = 0 */ ) {
  state_t new_state;
  int max_cycle = 8;
  do {
    flags &= ~ATM_SLEEP_FLAG;
    cycle();
    new_state = tiny_read_state( state_table + ( current * state_width ) + evt + ATM_ON_EXIT + 1 );
  } while ( --max_cycle && ( new_state == -1 || next_trigger != -1 ) );
  if ( new_state > -1 ) {
    next_trigger = evt;
    flags &= ~ATM_SLEEP_FLAG;
    cycle();  // Pick up the trigger
    flags &= ~ATM_SLEEP_FLAG;
    cycle();  // Process the state change
  }
  return *this;
}

TinyMachine& TinyMachine::begin( const tiny_state_t* tbl, int width ) {
  state_table = tbl;
  state_width = ATM_ON_EXIT + width + 2;
  flags &= ~ATM_SLEEP_FLAG;
  return *this;
}

// .cycle() Executes one cycle of a state machine
TinyMachine& TinyMachine::cycle( uint32_t time /* = 0 */ ) {
  uint32_t cycle_start = millis();
  do {
    if ( ( flags & ( ATM_SLEEP_FLAG | ATM_CYCLE_FLAG ) ) == 0 ) {
      flags |= ATM_CYCLE_FLAG;
      if ( next != -1 ) {
        action( ATM_ON_SWITCH );
        if ( current > -1 ) action( tiny_read_state( state_table + ( current * state_width ) + ATM_ON_EXIT ) );
        current = next;
        next = -1;
        state_millis = millis();
        action( tiny_read_state( state_table + ( current * state_width ) + ATM_ON_ENTER ) );
        if ( tiny_read_state( state_table + ( current * state_width ) + ATM_ON_LOOP ) == ATM_SLEEP ) {
          flags |= ATM_SLEEP_FLAG;
        } else {
          flags &= ~ATM_SLEEP_FLAG;
        }
      }
      tiny_state_t i = tiny_read_state( state_table + ( current * state_width ) + ATM_ON_LOOP );
      if ( i != -1 ) {
        action( i );
      }
      for ( i = ATM_ON_EXIT + 1; i < state_width; i++ ) {
        state_t next_state = tiny_read_state( state_table + ( current * state_width ) + i );
        if ( ( next_state != -1 ) && ( i == state_width - 1 || event( i - ATM_ON_EXIT - 1 ) || next_trigger == i - ATM_ON_EXIT - 1 ) ) {
          state( next_state );
          next_trigger = -1;
          break;
        }
      }
      flags &= ~ATM_CYCLE_FLAG;
    }
  } while ( millis() - cycle_start < time );
  return *this;
}

// FACTORY

// .calibrate() Distributes the machines in the inventory to the appropriate priority queues
void Factory::calibrate( void ) {
  // Reset all priority queues to empty lists
  for ( int8_t i = 0; i < ATM_NO_OF_QUEUES; i++ ) priority_root[i] = 0;
  // Walk the inventory list that contains all state machines in this factory
  Machine* m = inventory_root;
  while ( m ) {
    // Prepend every machine to the appropriate priority queue
    if ( m->prio < ATM_NO_OF_QUEUES ) {
      m->priority_next = priority_root[m->prio];
      priority_root[m->prio] = m;
    }
    m = m->inventory_next;
  }
  recalibrate = 0;
}

// .run( q ) Traverses an individual priority queue and cycles the machines in it once (except for queue 0)
void Factory::run( int q ) {
  Machine* m = priority_root[q];
  while ( m ) {
    if ( q > 0 && ( m->flags & ( ATM_SLEEP_FLAG | ATM_CYCLE_FLAG ) ) == 0 ) m->cycle();
    // Request a recalibrate if the prio field doesn't match the current queue
    if ( m->prio != q ) recalibrate = 1;
    // Move to the next machine
    m = m->priority_next;
  }
}

void Factory::runTiny() {
  TinyMachine* m;
  m = tiny_root;
  while ( m ) {
    if ( ( m->flags & ( ATM_SLEEP_FLAG | ATM_CYCLE_FLAG ) ) == 0 ) m->cycle();
    // Move to the next machine
    m = m->inventory_next;
    // if ( time > 0 && ( millis() - cycle_start ) < time ) break;
  }
}

// .add( machine ) Adds a state machine to the factory by prepending it to the inventory list
Factory& Factory::add( Machine& machine ) {
  machine.inventory_next = inventory_root;
  inventory_root = &machine;
  machine.factory = this;
  recalibrate = 1;
  return *this;
}

// .add( machine ) Adds a state machine to the factory by prepending it to the inventory list
Factory& Factory::add( TinyMachine& machine ) {
  machine.inventory_next = tiny_root;
  tiny_root = &machine;
  return *this;
}

// .find() Search the factory inventory for a machine by instance label
Machine* Factory::find( const char label[] ) {
  Machine* m = inventory_root;
  while ( m ) {
    if ( strcmp( label, m->inst_label ) == 0 ) {
      return m;
    }
    m = m->inventory_next;
  }
  return 0;
}

Factory& Factory::trigger( const char label[], int event /* = 0 */ ) {
  int l = 255;
  Machine* m = inventory_root;
  if ( label[strlen( label ) - 1] == '*' ) {
    l = strlen( label ) - 1;
  }
  if ( label[0] == '.' ) {
    l--;
    label++;
    while ( m ) {
      if ( strncmp( label, m->class_label, l ) == 0 ) {
        m->trigger( event );
      }
      m = m->inventory_next;
    }
  } else {
    while ( m ) {
      if ( strncmp( label, m->inst_label, l ) == 0 ) {
        m->trigger( event );
      }
      m = m->inventory_next;
    }
  }
  return *this;
}

int Factory::state( const char label[] ) {
  int r = 0;
  int l = 255;
  Machine* m = inventory_root;
  if ( label[strlen( label ) - 1] == '*' ) {
    l = strlen( label ) - 1;
  }
  if ( label[0] == '.' ) {
    l--;
    label++;
    while ( m ) {
      if ( strncmp( label, m->class_label, l ) == 0 ) {
        r += m->state();
      }
      m = m->inventory_next;
    }
  } else {
    while ( m ) {
      if ( strncmp( label, m->inst_label, l ) == 0 ) {
        r += m->state();
      }
      m = m->inventory_next;
    }
  }
  return r;
}

// .cycle() executes one factory cycle (runs all priority queues a certain number of times)
Factory& Factory::cycle( uint32_t time /* = 0 */ )  // Is it safe to allow recursion here???
{
  uint32_t cycle_start = millis();
  if ( recalibrate ) calibrate();
  do {
    runTiny();
    run( 1 );
    run( 2 );
    run( 1 );
    run( 2 );
    runTiny();
    run( 1 );
    run( 3 );
    run( 1 );
    run( 4 );
    runTiny();
    run( 1 );
    run( 2 );
    run( 1 );
    run( 3 );
    runTiny();
    run( 1 );
    run( 2 );
    run( 1 );
    run( 0 );
    runTiny();
  } while ( millis() - cycle_start < time );
  return *this;
}