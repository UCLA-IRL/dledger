use std::io;
use std::io::{Read, Write};
use std::mem;
use std::str;
use std::str::from_utf8;

// This function borrows a slice
fn call_host(func_name: &str, argument: &[u8]) -> Vec<u8>{
    //call
    io::stdout().write(func_name.as_bytes());
    let argument_length: [u8; 4] = [(argument.len() & 255) as u8, ((argument.len() >> 8) & 255) as u8,
        ((argument.len() >> 16) & 255) as u8, ((argument.len() >> 24) & 255) as u8];
    io::stdout().write(&argument_length);
    io::stdout().write(argument);
    io::stdout().flush();

    //get return
    let mut size_buf: [u8; 4] = [0; 4];
    io::stdin().read(&mut size_buf);
    let w = &size_buf as *const [u8] as *const [u32];
    let size: usize = unsafe { (&*w)[0] as usize };
    let mut buffer = vec![0; size];
    io::stdin().read(&mut buffer[0..size]);
    return buffer;
}

fn main() {
    eprintln!("ENTER wasi module");
    let mut size_buf: [u8; 4] = [0; 4];
    io::stdin().read(&mut size_buf);
    let w = &size_buf as *const [u8] as *const [u32];
    let size: usize = unsafe { (&*w)[0] as usize };
    let mut buffer = vec![0; size];
    io::stdin().read(&mut buffer[0..size]);
    eprintln!("WASM get: {}", from_utf8(&buffer).unwrap());

    let ret = call_host("GETB", "Argument from wasm".as_bytes());
    eprintln!("WASM getblock return: {}", from_utf8(&ret).unwrap());

    //done
    let ret = "Return from wasm";
    io::stdout().write("DONE".as_bytes());
    let ret_length: [u8; 4] = [(ret.len() & 255) as u8, ((ret.len() >> 8) & 255) as u8,
        ((ret.len() >> 16) & 255) as u8, ((ret.len() >> 24) & 255) as u8];
    io::stdout().write(&ret_length);
    io::stdout().write(ret.as_bytes());
}
