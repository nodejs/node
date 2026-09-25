const React = {
  createElement: () => {
    ("あ 🐕 🐕", throw Error("an error"));
  }
};

const profile = (
  <div>
    <img src="avatar.png" className="profile" />
    <h3>{["hello"]}</h3>
  </div>
);
