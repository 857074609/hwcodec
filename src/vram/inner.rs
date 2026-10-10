use crate::common::{DataFormat, DecodeCallback, EncodeCallback};
use std::os::raw::{c_int, c_void};

pub type NewEncoderCall = unsafe extern "C" fn(
    hdl: *mut c_void,
    luid: i64,
    codecID: i32,
    width: i32,
    height: i32,
    bitrate: i32,
    framerate: i32,
    gop: i32,
    qp_min: i32,
    qp_max: i32,
) -> *mut c_void;

// [LnDesk v302b hwcodec-force-i] VRAM 路径按设计不强制 IDR，但 EncodeCall 被
// amf/mfx/nv/ffmpeg 四套后端共用，ffmpeg_vram_encode 已是 6 参（含 force_i），
// 所以这里把共享签名统一成 6 参。amf/mfx/nv 的 encode 接受并忽略 force_i，
// 仅 ffmpeg_vram 真正使用它。Rust 侧 Encoder::encode 永远传 0（不强制 IDR）。
pub type EncodeCall = unsafe extern "C" fn(
    encoder: *mut c_void,
    tex: *mut c_void,
    callback: EncodeCallback,
    obj: *mut c_void,
    ms: i64,
    force_i: c_int,
) -> c_int;

pub type NewDecoderCall =
    unsafe extern "C" fn(device: *mut c_void, luid: i64, dataFormat: i32) -> *mut c_void;

pub type DecodeCall = unsafe extern "C" fn(
    decoder: *mut c_void,
    data: *mut u8,
    length: i32,
    callback: DecodeCallback,
    obj: *mut c_void,
) -> c_int;

pub type TestEncodeCall = unsafe extern "C" fn(
    outLuids: *mut i64,
    outVendors: *mut i32,
    maxDescNum: i32,
    outDescNum: *mut i32,
    dataFormat: i32,
    width: i32,
    height: i32,
    kbs: i32,
    framerate: i32,
    gop: i32,
    excludedLuids: *const i64,
    excludeFormats: *const i32,
    excludeCount: i32,
) -> c_int;

pub type TestDecodeCall = unsafe extern "C" fn(
    outLuids: *mut i64,
    outVendors: *mut i32,
    maxDescNum: i32,
    outDescNum: *mut i32,
    dataFormat: i32,
    data: *mut u8,
    length: i32,
    excludedLuids: *const i64,
    excludeFormats: *const i32,
    excludeCount: i32,
) -> c_int;

pub type IVCall = unsafe extern "C" fn(v: *mut c_void) -> c_int;

pub type IVICall = unsafe extern "C" fn(v: *mut c_void, i: i32) -> c_int;

pub struct EncodeCalls {
    pub new: NewEncoderCall,
    pub encode: EncodeCall,
    pub destroy: IVCall,
    pub test: TestEncodeCall,
    pub set_bitrate: IVICall,
    pub set_framerate: IVICall,
}
pub struct DecodeCalls {
    pub new: NewDecoderCall,
    pub decode: DecodeCall,
    pub destroy: IVCall,
    pub test: TestDecodeCall,
}

pub struct InnerEncodeContext {
    pub format: DataFormat,
}

pub struct InnerDecodeContext {
    pub data_format: DataFormat,
}
