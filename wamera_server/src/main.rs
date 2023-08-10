use std::fs::File;
use std::io::{self, Read};
use std::sync::Arc;
use std::time::{Instant, SystemTime};

use v4l::buffer::Type;
use v4l::io::traits::CaptureStream;
use v4l::prelude::*;
use v4l::video::capture::Parameters;
use v4l::video::Capture;
use v4l::{Format, FourCC};

use tokio::{
    io::{AsyncReadExt, AsyncWriteExt},
    sync::{mpsc, Mutex, RwLock},
};

use bytes::Bytes;

#[tokio::main]
async fn main() {
    let path = "/dev/video0";
    println!("Using device: {}\n", path);

    // Allocate 4 buffers by
    let buffer_count = 4;

    // Capture 4 frames by default
    let count = 4;

    let mut format: Format;
    let params: Parameters;

    let dev = RwLock::new(Device::with_path(path).unwrap());
    {
        let dev = dev.write().await;
        for availables in dev.enum_framesizes(FourCC::new(b"MJPG")).unwrap() {
            println!("{availables}")
        }
        format = dev.format().unwrap();
        params = dev.params().unwrap();

        // other format settings
        format.width = 1280;
        format.height = 720;

        // fallback to Motion-JPEG
        format.fourcc = FourCC::new(b"MJPG");
        format = dev.set_format(&format).unwrap();
        // if format.fourcc != FourCC::new(b"MJPG") {
        //     return Err(io::Error::new(
        //         io::ErrorKind::Other,
        //         "MJPG is not supported by the device, but required by this example!",
        //     ));
        // }
    }

    println!("Active format:\n{}", format);
    println!("Active parameters:\n{}", params);

    let dev = dev.write().await;

    // Setup a buffer stream
    let mut stream = MmapStream::with_buffers(&dev, Type::VideoCapture, buffer_count).unwrap();

    // warmup
    stream.next().unwrap();

    let (tx, rx) = mpsc::channel::<(u64, Bytes)>(32);

    let start = Instant::now();
    let mut megabytes_ps: f64 = 0.0;
    for i in 0..count {
        let t0 = Instant::now();
        let (buf, meta) = stream.next().unwrap();
        let duration_us = t0.elapsed().as_micros();
        let t1 = Instant::now();

        let cur = buf.len() as f64 / 1_048_576.0 * 1_000_000.0 / duration_us as f64;
        if i == 0 {
            megabytes_ps = cur;
        } else {
            // ignore the first measurement
            let prev = megabytes_ps * (i as f64 / (i + 1) as f64);
            let now = cur * (1.0 / (i + 1) as f64);
            megabytes_ps = prev + now;
        }

        println!("Buffer");
        println!("  sequence  : {}", meta.sequence);
        println!("  timestamp : {}", meta.timestamp);
        println!("  flags     : {}", meta.flags);
        println!("  length    : {}", buf.len());

        let buf2 = buf.to_vec();
        let data = Bytes::from(buf2);
        tx.send((instant_to_timestamp(t1), data)).await;
    }

    println!();
    println!("FPS: {}", count as f64 / start.elapsed().as_secs_f64());
    println!("MB/s: {}", megabytes_ps);

    // tokio::spawn(move || loop {
    //     let t0 = Instant::now();
    //     let data = rx.recv().unwrap();
    //     let t1 = Instant::now();

    //     print!(
    //         "\rframe: {}, delay: {}ms",
    //         data.0,
    //         t1.duration_since(t0).as_millis(),
    //     );

    //     if FRAME_COUNT.load(std::sync::atomic::Ordering::Relaxed) == MAX_FRAME_COUNT {
    //         break;
    //     }
    // });
}

fn instant_to_timestamp(instant: Instant) -> u64 {
    let duration = instant.elapsed();
    let since_epoch = SystemTime::now()
        .duration_since(SystemTime::UNIX_EPOCH)
        .expect("Time went backwards");

    (since_epoch + duration).as_secs() as u64
}
