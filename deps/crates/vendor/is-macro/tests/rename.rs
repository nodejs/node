use is_macro::Is;
#[derive(Debug, Is)]
pub enum Enum {
    #[is(name = "video_mp4")]
    VideoMp4,
}

#[test]
fn test() {
    assert!(Enum::VideoMp4.is_video_mp4());
}
