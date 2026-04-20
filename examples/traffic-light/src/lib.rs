#![no_std]

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

#[repr(C)]
#[derive(Clone, Copy, PartialEq)]
pub enum SysEvent {
    BtnPressed = 0,
}

unsafe extern "C" {
    fn task_delay(ticks: u32);
    fn queue_receive(
        queue: *mut core::ffi::c_void,
        item: *mut core::ffi::c_void,
        timeout: u32,
    ) -> u32;
    fn queue_send(
        queue: *mut core::ffi::c_void,
        item: *const core::ffi::c_void,
        timeout: u32,
    ) -> u32;
    fn hardware_set_traffic_light(active_led: u32);
    fn hardware_set_blue_led(state: u32);
    fn hardware_read_button() -> u32;
}

// LED Constants (match C side)
const LED_GREEN: u32 = 1 << 12;
const LED_ORANGE: u32 = 1 << 13;
const LED_RED: u32 = 1 << 14;

static mut IS_GREEN_LIGHT: bool = true;

#[unsafe(no_mangle)]
pub extern "C" fn rust_task_button_poll(queue_handle: *mut core::ffi::c_void) {
    let mut last_state = false;
    loop {
        let current_state = unsafe { hardware_read_button() } != 0;
        if current_state && !last_state {
            let evt = SysEvent::BtnPressed;
            unsafe {
                queue_send(
                    queue_handle,
                    &evt as *const _ as *const core::ffi::c_void,
                    0,
                );
            }
        }
        last_state = current_state;
        unsafe { task_delay(50) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_task_traffic_light(queue_handle: *mut core::ffi::c_void) {
    loop {
        // GREEN
        unsafe {
            IS_GREEN_LIGHT = true;
            hardware_set_traffic_light(LED_GREEN);
        }
        let mut evt = SysEvent::BtnPressed;
        
        // Wait indefinitely for button press
        unsafe {
            queue_receive(queue_handle, &mut evt as *mut _ as *mut core::ffi::c_void, 0xFFFFFFFF);
        }

        // YELLOW
        unsafe {
            IS_GREEN_LIGHT = false;
            hardware_set_traffic_light(LED_ORANGE);
            
            // Absolutely strictly wait 3s without extending
            task_delay(3000);
            
            // Flush any button presses that accumulated during the wait instantly
            while queue_receive(queue_handle, &mut evt as *mut _ as *mut core::ffi::c_void, 0) == 0 {
                // Keep pulling until empty
            }
        }

        // RED
        unsafe {
            hardware_set_traffic_light(LED_RED);
        }
        
        // Wait indefinitely for button press
        loop {
            let res = unsafe { queue_receive(queue_handle, &mut evt as *mut _ as *mut core::ffi::c_void, 0xFFFFFFFF) };
            if res == 0 && evt == SysEvent::BtnPressed {
                break;
            }
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_task_car_sim(_param: *mut core::ffi::c_void) {
    let mut lfsr: u32 = 0xACE1;
    loop {
        let is_green = unsafe { core::ptr::read_volatile(&raw const IS_GREEN_LIGHT) };
        if is_green {
            let bit = ((lfsr) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
            lfsr = (lfsr >> 1) | (bit << 15);

            if (lfsr & 0x3) == 0 {
                unsafe {
                    hardware_set_blue_led(1);
                    task_delay(100);
                    hardware_set_blue_led(0);
                }
            }
        }
        unsafe { task_delay(800) };
    }
}
