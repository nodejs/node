/*---
esid: prod-OptionalExpression
features: [optional-chaining]
---*/
const obj = {
  a: {
    b: 22
  }
};

function fn() {
  return {};
}

setTimeout(err => {
  var _obj$a;

  // OptionalExpression (MemberExpression OptionalChain) OptionalChain
  if ((obj === null || obj === void 0 ? void 0 : (_obj$a = obj.a) === null || _obj$a === void 0 ? void 0 : _obj$a.b) === 22) throw Error('an exception');
}, 5);
//# sourceMappingURL=data:application/json;charset=utf-8;base64,eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbImJhYmVsLXRocm93LW9yaWdpbmFsLmpzIl0sIm5hbWVzIjpbIm9iaiIsImEiLCJiIiwiZm4iLCJzZXRUaW1lb3V0IiwiZXJyIiwiRXJyb3IiXSwibWFwcGluZ3MiOiJBQUFBOzs7O0FBS0EsTUFBTUEsR0FBRyxHQUFHO0FBQ1ZDLEVBQUFBLENBQUMsRUFBRTtBQUNEQyxJQUFBQSxDQUFDLEVBQUU7QUFERjtBQURPLENBQVo7O0FBTUEsU0FBU0MsRUFBVCxHQUFlO0FBQ2IsU0FBTyxFQUFQO0FBQ0Q7O0FBRURDLFVBQVUsQ0FBRUMsR0FBRCxJQUFTO0FBQUE7O0FBQ2xCO0FBQ0EsTUFBSSxDQUFBTCxHQUFHLFNBQUgsSUFBQUEsR0FBRyxXQUFILHNCQUFBQSxHQUFHLENBQUVDLENBQUwsa0RBQVFDLENBQVIsTUFBYyxFQUFsQixFQUFzQixNQUFNSSxLQUFLLENBQUMsY0FBRCxDQUFYO0FBQ3ZCLENBSFMsRUFHUCxDQUhPLENBQVYiLCJzb3VyY2VzQ29udGVudCI6WyIvKi0tLVxuZXNpZDogcHJvZC1PcHRpb25hbEV4cHJlc3Npb25cbmZlYXR1cmVzOiBbb3B0aW9uYWwtY2hhaW5pbmddXG4tLS0qL1xuXG5jb25zdCBvYmogPSB7XG4gIGE6IHtcbiAgICBiOiAyMlxuICB9XG59O1xuXG5mdW5jdGlvbiBmbiAoKSB7XG4gIHJldHVybiB7fTtcbn1cblxuc2V0VGltZW91dCgoZXJyKSA9PiB7XG4gIC8vIE9wdGlvbmFsRXhwcmVzc2lvbiAoTWVtYmVyRXhwcmVzc2lvbiBPcHRpb25hbENoYWluKSBPcHRpb25hbENoYWluXG4gIGlmIChvYmo/LmE/LmIgPT09IDIyKSB0aHJvdyBFcnJvcignYW4gZXhjZXB0aW9uJyk7XG59LCA1KTtcblxuIl19
