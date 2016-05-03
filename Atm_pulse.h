
#ifndef Atm_pulse_h
#define Atm_pulse_h

#include <Automaton.h>

// Detects a pulse (LOW -> HIGH ) on a digital pin with a minimum duration in ms
// On detection another machine is messaged or a callback is fired

typedef void (*pulsecb_t)( int idx, uint16_t cnt );

class Atm_pulse: public Machine {

  public:
    Atm_pulse( void ) : Machine() { class_label = "PULSE"; };

    short pin;     
    int state_high, state_low;
    atm_timer_millis timer;
    union {
      struct { // ATM_USR1_FLAG - callback
        void (*_callback)( int idx, uint16_t cnt );
        int _callback_idx;
        uint16_t _callback_count;
      };
      struct { // ATM_USR2_FLAG - machine trigger
        Machine * _client_machine;
        state_t _client_machine_event;
      };
      struct { // ATM_USR3_FLAG - factory trigger
        const char * _client_label;
        state_t _client_label_event;
      };
    };

    enum { IDLE, WAIT, PULSE };
    enum { EVT_TIMER, EVT_HIGH, EVT_LOW, ELSE };
    enum { ACT_PULSE };
	
    Atm_pulse & begin( int attached_pin, int minimum_duration );
    Atm_pulse & trace( Stream & stream );
    int event( int id ); 
    void action( int id ); 
    Atm_pulse & onPulse( pulsecb_t callback, int idx = 0 );
    Atm_pulse & onPulse( Machine & machine, int event = 0 );
    Atm_pulse & onPulse( const char * label, int event = 0 );
};

// TinyMachine version

class Att_pulse: public TinyMachine {

  public:
    Att_pulse( void ) : TinyMachine() { };

    short pin;     
    int state_high, state_low;
    atm_timer_millis timer;
    union {
      struct { // ATM_USR1_FLAG - callback
        void (*_callback)( int idx, uint16_t cnt );
        int _callback_idx;
        uint16_t _callback_count;
      };
      struct { // ATM_USR2_FLAG - machine trigger
        TinyMachine * _client_machine;
        state_t _client_machine_event;
      };
      struct { // ATM_USR3_FLAG - factory trigger
        const char * _client_label;
        state_t _client_label_event;
      };
    };

    enum { IDLE, WAIT, PULSE };
    enum { EVT_TIMER, EVT_HIGH, EVT_LOW, ELSE };
    enum { ACT_PULSE };
	
    Att_pulse & begin( int attached_pin, int minimum_duration );
    Att_pulse & trace( Stream & stream );
    int event( int id ); 
    void action( int id ); 
    Att_pulse & onPulse( pulsecb_t callback, int idx = 0 );
    Att_pulse & onPulse( TinyMachine & machine, int event = 0 );
};




#endif

