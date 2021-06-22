import v8 from "v8";
import cloneDeep from "./clone-deep-browser";

export default function (value) {
  if (v8.deserialize && v8.serialize) {
    return v8.deserialize(v8.serialize(value));
  }
  return cloneDeep(value);
}
