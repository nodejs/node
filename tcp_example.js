
TCP.connect ({
  host: "google.com",
  port: 80,
  connected: function () {
    log("connected to google.com");
  }
});

