use std::env;
use std::ffi::c_void;
use std::fs;
use std::io::{self, Read};
use std::path::PathBuf;
use std::process::ExitCode;

use milo_parser::{CALLBACK_ACTIVE_ALL, Parser, STATE_ERROR};

struct Options {
  file: Option<PathBuf>,
  mode: Mode,
}

#[derive(Clone, Copy, PartialEq)]
enum Mode {
  Autodetect,
  Request,
  Response,
}

struct Context {
  output: Vec<String>,
}

enum RunError {
  Usage(String),
  Io(String),
}

fn print_usage() {
  eprintln!("Usage: milo-parser [-f|--file PATH] [-o|--request | -i|--response]");
}

fn parse_args() -> Result<Options, String> {
  let mut args = env::args().skip(1);
  let mut options = Options {
    file: None,
    mode: Mode::Autodetect,
  };

  while let Some(arg) = args.next() {
    match arg.as_str() {
      "-f" | "--file" => {
        let path = args.next().ok_or_else(|| format!("Missing value for {arg}"))?;
        options.file = Some(PathBuf::from(path));
      }
      "-o" | "--request" => {
        if options.mode != Mode::Autodetect {
          return Err("Cannot use both request and response mode".into());
        }

        options.mode = Mode::Request;
      }
      "-i" | "--response" => {
        if options.mode != Mode::Autodetect {
          return Err("Cannot use both request and response mode".into());
        }

        options.mode = Mode::Response;
      }
      "-h" | "--help" => {
        print_usage();
        std::process::exit(0);
      }
      _ => return Err(format!("Unknown argument: {arg}")),
    }
  }

  Ok(options)
}

fn read_input(options: &Options) -> io::Result<Vec<u8>> {
  match &options.file {
    Some(path) => fs::read(path),
    None => {
      let mut input = Vec::new();
      io::stdin().read_to_end(&mut input)?;
      Ok(input)
    }
  }
}

fn quote(value: &str) -> String {
  let mut quoted = String::with_capacity(value.len() + 2);
  quoted.push('"');

  for c in value.chars() {
    match c {
      '"' => quoted.push_str("\\\""),
      '\\' => quoted.push_str("\\\\"),
      '\n' => quoted.push_str("\\n"),
      '\r' => quoted.push_str("\\r"),
      '\t' => quoted.push_str("\\t"),
      _ => quoted.push(c),
    }
  }

  quoted.push('"');
  quoted
}

fn append_output(parser: &mut Parser, line: String) {
  let mut context = unsafe { Box::from_raw(parser.context as *mut Context) };
  context.output.push(line);
  let _ = Box::into_raw(context);
}

fn event(parser: &mut Parser, offset: usize, size: usize, name: &str) {
  append_output(parser, format!("offset={offset} size={size} event={name}"));
}

fn on_error(parser: &mut Parser, offset: usize, size: usize) {
  append_output(
    parser,
    format!(
      "offset={offset} size={size} event=error error={} description={}",
      parser.error_code_str(),
      quote(parser.error_description_str())
    ),
  );
}

fn on_finish(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "finish"); }

fn on_message_start(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "message_start"); }

fn on_message_complete(parser: &mut Parser, offset: usize, size: usize) {
  event(parser, offset, size, "message_complete");
}

fn on_request(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "request"); }

fn on_response(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "response"); }

fn on_reset(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "reset"); }

fn on_method(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "method"); }

fn on_url(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "url"); }

fn on_protocol(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "protocol"); }

fn on_version(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "version"); }

fn on_status(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "status"); }

fn on_reason(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "reason"); }

fn on_header_name(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "header_name"); }

fn on_header_value(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "header_value"); }

fn on_headers(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "headers"); }

fn on_connect(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "connect"); }

fn on_upgrade(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "upgrade"); }

fn on_chunk_length(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "chunk_length"); }

fn on_chunk_extension_name(parser: &mut Parser, offset: usize, size: usize) {
  event(parser, offset, size, "chunk_extension_name");
}

fn on_chunk_extension_value(parser: &mut Parser, offset: usize, size: usize) {
  event(parser, offset, size, "chunk_extension_value");
}

fn on_chunk(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "chunk"); }

fn on_body(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "body"); }

fn on_data(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "data"); }

fn on_trailer_name(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "trailer_name"); }

fn on_trailer_value(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "trailer_value"); }

fn on_trailers(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "trailers"); }

fn on_state_change(parser: &mut Parser, offset: usize, size: usize) { event(parser, offset, size, "state_change"); }

fn create_parser(mode: Mode) -> Parser {
  let mut parser = Parser::new();
  let context = Box::new(Context { output: Vec::new() });
  parser.context = Box::into_raw(context) as *mut c_void;
  parser.active_callbacks = CALLBACK_ACTIVE_ALL;

  match mode {
    Mode::Autodetect => {
      parser.autodetect = true;
    }
    Mode::Request => {
      parser.autodetect = false;
      parser.is_request = true;
    }
    Mode::Response => {
      parser.autodetect = false;
      parser.is_request = false;
    }
  }

  parser.callbacks.on_error = on_error;
  parser.callbacks.on_finish = on_finish;
  parser.callbacks.on_message_start = on_message_start;
  parser.callbacks.on_message_complete = on_message_complete;
  parser.callbacks.on_request = on_request;
  parser.callbacks.on_response = on_response;
  parser.callbacks.on_reset = on_reset;
  parser.callbacks.on_method = on_method;
  parser.callbacks.on_url = on_url;
  parser.callbacks.on_protocol = on_protocol;
  parser.callbacks.on_version = on_version;
  parser.callbacks.on_status = on_status;
  parser.callbacks.on_reason = on_reason;
  parser.callbacks.on_header_name = on_header_name;
  parser.callbacks.on_header_value = on_header_value;
  parser.callbacks.on_headers = on_headers;
  parser.callbacks.on_connect = on_connect;
  parser.callbacks.on_upgrade = on_upgrade;
  parser.callbacks.on_chunk_length = on_chunk_length;
  parser.callbacks.on_chunk_extension_name = on_chunk_extension_name;
  parser.callbacks.on_chunk_extension_value = on_chunk_extension_value;
  parser.callbacks.on_chunk = on_chunk;
  parser.callbacks.on_body = on_body;
  parser.callbacks.on_data = on_data;
  parser.callbacks.on_trailer_name = on_trailer_name;
  parser.callbacks.on_trailer_value = on_trailer_value;
  parser.callbacks.on_trailers = on_trailers;
  parser.callbacks.on_state_change = on_state_change;

  parser
}

fn run() -> Result<bool, RunError> {
  let options = parse_args().map_err(RunError::Usage)?;
  let input = read_input(&options).map_err(|e| RunError::Io(e.to_string()))?;
  let mut parser = create_parser(options.mode);

  parser.parse(input.as_ptr(), input.len());

  if parser.state != STATE_ERROR {
    parser.finish();
  }

  let failed = parser.state == STATE_ERROR;
  let context = unsafe { Box::from_raw(parser.context as *mut Context) };

  for line in &context.output {
    println!("{line}");
  }

  Ok(failed)
}

fn main() -> ExitCode {
  match run() {
    Ok(false) => ExitCode::SUCCESS,
    Ok(true) => ExitCode::from(1),
    Err(RunError::Usage(error)) => {
      eprintln!("{error}");
      print_usage();
      ExitCode::from(2)
    }
    Err(RunError::Io(error)) => {
      eprintln!("{error}");
      ExitCode::from(1)
    }
  }
}
