require('dotenv').config();
const express = require("express");
const cors = require("cors");
const fetch = (...args) => import("node-fetch").then(({ default: fetch }) => fetch(...args));

const app = express();
app.use(cors());
app.use(express.json());

const API_KEY = process.env.SMS_API_KEY;

// test endpoint
app.get("/", (req, res) => {
  res.send("SMS server is running ✅");
});

// send sms endpoint
app.post("/send-sms", async (req, res) => {
  const { to, message } = req.body;
  const url = `https://api.sms.net.bd/sendsms?api_key=${API_KEY}&msg=${encodeURIComponent(
    message
  )}&to=${to}`;

  try {
    const response = await fetch(url);
    const result = await response.json();
    res.json(result);
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

app.listen(3000, () => console.log("🚀 SMS server running on port 3000"));
