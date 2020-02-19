#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
static struct lock *intersection_lock;
static struct cv *cvs[4];
static int count[4];
/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 */
void
intersection_sync_init(void)
{
  intersection_lock = lock_create("lk_intersection");
  for(int i = 0; i < 4; i++){
    count[i]=0;
  }
  cvs[0] = cv_create("cv_N");
  cvs[1] = cv_create("cv_E");
  cvs[2] = cv_create("cv_S");
  cvs[3] = cv_create("cv_W");
  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 */
void
intersection_sync_cleanup(void)
{
  lock_destroy(intersection_lock);
  for(int i = 0; i < 4; i++){
    cv_destroy(cvs[i]);
  }
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  (void)destination;
  lock_acquire(intersection_lock);

  while((count[0] != 0 && origin != 0) ||   // If the count from an origin is not 0, this car must wait
        (count[1] != 0 && origin != 1) ||   // unless the cars in the intersection are from this threads 
        (count[2] != 0 && origin != 2) ||   // origin. This while loop will spin until all origins are  
        (count[3] != 0 && origin != 3))     // empty (0) except its own origin.
  {
    cv_wait(cvs[origin], intersection_lock);  
  }
  count[origin]++;
  lock_release(intersection_lock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  (void)destination;
  lock_acquire(intersection_lock);
  count[origin]--;
  for(int i=0; i<4; i++){
    cv_broadcast(cvs[i], intersection_lock);
  }
  lock_release(intersection_lock);
}
