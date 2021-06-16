#pragma once

#ifndef MAX_SEGMENTS
#  define MAX_SEGMENTS (32)  // number of line segments to buffer ahead. must be a power of two.
#endif

#define SEGMOD(x) ((x) & (MAX_SEGMENTS - 1))


typedef struct {
  int32_t delta_steps;  // total steps for this move.
  int32_t step_count;   // current motor position, in steps.
  int32_t delta_units;  // in some systems it's mm, in others it's degrees.
  uint32_t absdelta;
  int dir;
#if MACHINE_STYLE == SIXI
  float expectedPosition;
  float positionStart;
  float positionEnd;
#endif
} Muscle;


enum BlockFlagBits : uint8_t { 
    BIT_FLAG_NOMINAL,
    BIT_FLAG_RECALCULATE
};

enum BlockFlagMask : uint8_t {
  BLOCK_FLAG_NOMINAL     = _BV(BIT_FLAG_NOMINAL),
  BLOCK_FLAG_RECALCULATE = _BV(BIT_FLAG_RECALCULATE),
};


typedef struct {
  Muscle a[NUM_MUSCLES];

  float distance;         // units
  float nominal_speed;    // units/s
  float entry_speed;      // units/s
  float entry_speed_max;  // units/s
  float acceleration;     // units/sec^2

  uint32_t steps_total;  // steps
  uint32_t steps_taken;  // steps
  uint32_t accel_until;  // steps
  uint32_t decel_after;  // steps

  uint32_t nominal_rate;               // steps/s
  uint32_t initial_rate;               // steps/s
  uint32_t final_rate;                 // steps/s
  uint32_t acceleration_steps_per_s2;  // steps/s^2
  uint32_t acceleration_rate;  // ?

  uint8_t flags;
} Segment;


class Planner {
  public:
  static Segment blockBuffer[MAX_SEGMENTS];
  static volatile int block_buffer_head, 
               block_buffer_nonbusy,
               block_buffer_planned,
               block_buffer_tail;
  static int first_segment_delay;

  static float previous_nominal_speed;
  static float previous_safe_speed;
  static float previous_speed[NUM_MUSCLES];


  FORCE_INLINE static uint8_t movesPlanned() {
    return SEGMOD(block_buffer_head - block_buffer_tail);
  }

  FORCE_INLINE static uint8_t movesFree() {
      return MAX_SEGMENTS - 1 - movesPlanned();
  }

  FORCE_INLINE static uint8_t movesPlannedNotBusy() {
    return SEGMOD(block_buffer_head - block_buffer_nonbusy);
  }


  FORCE_INLINE static int getNextBlock(int i) {
    return SEGMOD(i + 1);
  }

  FORCE_INLINE static int getPrevBlock(int i) {
    return SEGMOD(i - 1);
  }

  FORCE_INLINE void releaseCurrentBlock() {
    block_buffer_tail = getNextBlock(block_buffer_tail);
  }

  FORCE_INLINE Segment *getCurrentBlock() {
    if (block_buffer_tail == block_buffer_head) return NULL;

    if (first_segment_delay > 0) {
      --first_segment_delay;
      if (movesPlanned() < 3 && first_segment_delay) return NULL;
      first_segment_delay=0;
    }

    Segment *block = &blockBuffer[block_buffer_tail];

    if( TEST(block->flags,BIT_FLAG_RECALCULATE) ) return NULL;  // wait, not ready

    block_buffer_nonbusy = getNextBlock(block_buffer_tail);
    if (block_buffer_tail == block_buffer_planned) {
      block_buffer_planned = block_buffer_nonbusy;
    }

    return block;
  }
  
  FORCE_INLINE static Segment *getNextFreeBlock(uint8_t &next_buffer_head,const uint8_t count=1) {
    // get the next available spot in the segment buffer
    while (getNextBlock(block_buffer_head) == block_buffer_tail) {
      // the segment buffer is full, we are way ahead of the motion system.  wait here.
      meanwhile();
    }

    next_buffer_head = getNextBlock(block_buffer_head);
    return &blockBuffer[block_buffer_head];
  }

  void zeroSpeeds();
  void wait_for_empty_segment_buffer();

  void addSteps(Segment *newBlock,const float *const target_position, float fr_units_s, float longest_distance);
  void addSegment(const float *const target_position, float fr_units_s, float millimeters);
  void bufferLine(float *pos, float new_feed_rate_units);
  void bufferArc(float cx, float cy, float *destination, char clockwise, float new_feed_rate_units);

  // return 1 if buffer is full, 0 if it is not.
  bool segmentBufferFull() {
    int next_segment = getNextBlock(block_buffer_head);
    return (next_segment == block_buffer_tail);
  }

  float max_speed_allowed(const float &acc, const float &target_velocity, const float &distance);

  void recalculate_reverse_kernel(Segment *const current, const Segment *next);
  void recalculate_reverse();
  void recalculate_forward_kernel(const Segment *prev, Segment *const current,uint8_t block_index);
  void recalculate_forward();
  void recalculate_acceleration();

  float estimate_acceleration_distance(const float &initial_rate, const float &target_rate, const float &accel);
  int intersection_distance(const float &start_rate, const float &end_rate, const float &accel, const float &distance);
  void segment_update_trapezoid(Segment *s, const float &entry_factor, const float &exit_factor);
  void recalculate_trapezoids();

  void describeAllSegments();
  void segmentReport(Segment &new_seg);

  void estop();
};

extern Planner planner;