pub const MULTIPLIER: u64 = 0x5DEECE66D;
pub const ADDEND: u64 = 0xB;
pub const MASK: u64 = (1_u64 << 48) - 1;

#[derive(Clone, Debug)]
pub struct JavaRandom {
    seed: u64,
}

impl JavaRandom {
    pub fn new(seed: i64) -> Self {
        Self {
            seed: ((seed as u64) ^ MULTIPLIER) & MASK,
        }
    }

    fn next_bits(&mut self, bits: u32) -> u32 {
        self.seed = (self.seed.wrapping_mul(MULTIPLIER).wrapping_add(ADDEND)) & MASK;
        (self.seed >> (48 - bits)) as u32
    }

    pub fn next_int(&mut self) -> i32 {
        self.next_bits(32) as i32
    }

    pub fn next_double(&mut self) -> f64 {
        let high = (self.next_bits(26) as u64) << 27;
        let low = self.next_bits(27) as u64;
        ((high + low) as f64) / ((1_u64 << 53) as f64)
    }

    pub fn next_int_bound(&mut self, bound: i32) -> i32 {
        assert!(bound > 0, "bound must be positive");
        if (bound & -bound) == bound {
            return (((bound as i64) * (self.next_bits(31) as i64)) >> 31) as i32;
        }

        loop {
            let bits = self.next_bits(31) as i32;
            let value = bits % bound;
            if bits - value + (bound - 1) >= 0 {
                return value;
            }
        }
    }

    pub fn next_int_range(&mut self, origin: i32, bound: i32) -> i32 {
        assert!(origin < bound, "origin must be < bound");
        let mut value = self.next_int();
        let span = bound - origin;
        let mask = span - 1;
        if (span & mask) == 0 {
            return (value & mask) + origin;
        }

        if span > 0 {
            let mut unsigned = ((value as u32) >> 1) as i32;
            loop {
                value = unsigned % span;
                if unsigned.wrapping_add(mask).wrapping_sub(value) >= 0 {
                    return value + origin;
                }
                unsigned = ((self.next_int() as u32) >> 1) as i32;
            }
        }

        while value < origin || value >= bound {
            value = self.next_int();
        }
        value
    }

    pub fn shuffle<T>(&mut self, slice: &mut [T]) {
        let mut idx = slice.len();
        while idx > 1 {
            let swap_with = self.next_int_bound(idx as i32) as usize;
            slice.swap(idx - 1, swap_with);
            idx -= 1;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::JavaRandom;

    #[test]
    fn reproduces_known_values() {
        let mut rng = JavaRandom::new(12345);
        assert_eq!(rng.next_int_bound(1000), 251);
        assert_eq!(rng.next_int_bound(1000), 80);
        let value = rng.next_double();
        assert!((value - 0.932993485288541).abs() < 1e-15);
    }

    #[test]
    fn matches_java_range_semantics() {
        let mut rng = JavaRandom::new(1);
        let values = (0..8).map(|_| rng.next_int_range(2, 4)).collect::<Vec<_>>();
        assert_eq!(values, vec![3, 2, 3, 2, 2, 2, 3, 3]);
    }
}
