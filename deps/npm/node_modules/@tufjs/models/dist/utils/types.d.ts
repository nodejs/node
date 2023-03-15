export type JSONObject = {
    [key: string]: JSONValue;
};
export type JSONValue = null | boolean | number | string | JSONValue[] | JSONObject;
