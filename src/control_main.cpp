#include <iostream>
#include <string>

#include <argparse/argparse.hpp>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <plog/Log.h>
#include <zmq.hpp>

#include "consts.hpp"
#include "messages.hpp"

using namespace messages;

int main(int argc, char *argv[]) {
  static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
  plog::init(plog::debug, &consoleAppender);

  argparse::ArgumentParser program("led-matrix-zmq-control");
  program.add_description("Send control messages to led-matrix-zmq-server");
  program.add_argument("--control-endpoint").default_value(consts::DEFAULT_CONTROL_ENDPOINT);

  argparse::ArgumentParser set_brightness_command("set-brightness");
  set_brightness_command.add_description("Set the brightness");
  set_brightness_command.add_argument("brightness")
      .help("Brightness level (0%-100%)")
      .scan<'i', int>();
  argparse::ArgumentParser set_temperature_command("set-temperature");
  set_temperature_command.add_description("Set the color temperature");
  set_temperature_command.add_argument("temperature")
      .help("Temperature level (2000K-6500K)")
      .scan<'i', int>();

  argparse::ArgumentParser get_brightness_command("get-brightness");
  get_brightness_command.add_description("Get the brightness");
  argparse::ArgumentParser get_temperature_command("get-temperature");
  get_temperature_command.add_description("Get the color temperature");

  program.add_subparser(get_brightness_command);
  program.add_subparser(set_brightness_command);
  program.add_subparser(get_temperature_command);
  program.add_subparser(set_temperature_command);

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  zmq::context_t ctx;
  zmq::socket_t sock(ctx, ZMQ_REQ);
  sock.set(zmq::sockopt::linger, 0);
  sock.set(zmq::sockopt::rcvtimeo, 1000);
  sock.set(zmq::sockopt::sndtimeo, 1000);
  sock.connect(program.get("--control-endpoint"));

  zmq::message_t req, resp;

  if (program.is_subcommand_used(set_brightness_command)) {
    const auto brightness = set_brightness_command.get<int>("brightness");
    PLOG_INFO << "Setting brightness to " << brightness << "%";

    SetBrightnessRequest control_req;
    control_req.type = ControlRequestType::SetBrightness;
    control_req.args.brightness = brightness;

    req = zmq::message_t(&control_req, sizeof(control_req));
    sock.send(req, zmq::send_flags::none);
    static_cast<void>(sock.recv(resp, zmq::recv_flags::none));
  } else if (program.is_subcommand_used(set_temperature_command)) {
    const auto temperature = set_temperature_command.get<int>("temperature");
    PLOG_INFO << "Setting temperature to " << temperature << "K";

    SetTemperatureRequest control_req;
    control_req.type = ControlRequestType::SetTemperature;
    control_req.args.temperature = temperature;

    req = zmq::message_t(&control_req, sizeof(control_req));
    sock.send(req, zmq::send_flags::none);
    static_cast<void>(sock.recv(resp, zmq::recv_flags::none));
  } else if (program.is_subcommand_used(get_brightness_command)) {
    const GetBrightnessRequest control_req = {
        .type = ControlRequestType::GetBrightness,
        .args = {},
    };

    req = zmq::message_t(&control_req, sizeof(control_req));
    sock.send(req, zmq::send_flags::none);

    static_cast<void>(sock.recv(resp, zmq::recv_flags::none));
    const BrightnessResponse *control_resp =
        reinterpret_cast<const BrightnessResponse *>(resp.data());

    PLOG_INFO << "Brightness: " << std::to_string(control_resp->args.brightness) << "%";
  } else if (program.is_subcommand_used(get_temperature_command)) {
    const GetTemperatureRequest control_req = {
        .type = ControlRequestType::GetTemperature,
        .args = {},
    };

    req = zmq::message_t(&control_req, sizeof(control_req));
    sock.send(req, zmq::send_flags::none);

    static_cast<void>(sock.recv(resp, zmq::recv_flags::none));
    const TemperatureResponse *control_resp =
        reinterpret_cast<const TemperatureResponse *>(resp.data());

    PLOG_INFO << "Temperature: " << control_resp->args.temperature;
  } else {
    std::cerr << program;
    return 1;
  }

  return 0;
}
