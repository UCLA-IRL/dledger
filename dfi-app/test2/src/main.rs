use std::io;
use std::io::{Read, Write};

fn main() {
    eprintln!("ENTER wasi module");
    let mut buffer: [u8; 8800] = [0; 8800];
    io::stdin().read(&mut buffer[0..4]);
    let mut size_ptr = &buffer[0..4] as *const [u8] as *const [u32];
    let mut size = unsafe {(*size_ptr)[0]} as usize;
    io::stdin().read(&mut buffer[4..size+4]);

    // get
    io::stdout().write("GET".as_bytes());
    io::stdout().write(& buffer[0..size+4]);
    io::stdout().flush();
    //get return
    io::stdin().read(&mut buffer[0..4]);
    size_ptr = &buffer[0..4] as *const [u8] as *const [u32];
    size = unsafe {(*size_ptr)[0]} as usize;
    io::stdin().read(&mut buffer[4..size+4]);

    //done
    io::stdout().write("DNE".as_bytes());
    io::stdout().write(& buffer[0..size+4]);
}
