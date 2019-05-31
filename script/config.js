var skip_samples = 1;    // Skip first samples
var cut_samples = 1666600; // Maximum  samples
var avgSamples = 64; // Moving average window


var curveArray = [
    {"curveTitle":"Ambient temperature","channel":"ch17",	"offset":0,		"scale":300,	"group":0,	"tspan":1},
    {"curveTitle":"Agilent 34410A",	"channel":"ch1",	"offset":-0.00004,	"scale":1,	"group":0,	"tspan":0},
    {"curveTitle":"Keithley DMM6500",	"channel":"ch2",	"offset":0.00004,	"scale":1,	"group":0,	"tspan":0},
    {"curveTitle":"Keysight E36313A",	"channel":"ch3",	"offset":0.005,		"scale":1000,	"group":0,	"tspan":0}
  ];

var margin = {top: 40, right: 190, bottom: 20, left: 70},
    width = 1550,
    width2 = 1100,
    height = 1050 - margin.top - margin.bottom;
    height2 = 550 - margin.top - margin.bottom;

var circle_size =  2; // Bubble size
var circle_op = 0.4;  // Bubble transparent
var line_op = 1.0;    // Line transparent
var axis_tick = 30;   // Tick of Y axis

