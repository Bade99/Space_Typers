#pragma once

struct rc2 {
    v2 center;
    v2 radius;

    v2 get_min() {
        v2 res = center - radius;
        return res;
    }

    v2 get_max() {
        v2 res = center + radius;
        return res;
    }
};

rc2 rc_min_max(v2 min, v2 max) { //NOTE REMEMBER: I like this idea
    rc2 res;
    res.center = (max + min) / 2.f;
    res.radius = (max - min) / 2.f;
    return res;
}

rc2 rc_center_radius(v2 center, v2 radius) {
    rc2 res;
    res.center = center;
    res.radius = radius;
    return res;
}

bool is_in_rc(v2 p, rc2 r) {
    //NOTE: we dont include the final coordinate point
    bool res = p.x >= r.get_min().x && p.x < r.get_max().x&& p.y >= r.get_min().y && p.y < r.get_max().y;
    return res;
}