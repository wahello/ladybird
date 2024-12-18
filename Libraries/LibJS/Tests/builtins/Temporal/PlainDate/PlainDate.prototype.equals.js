describe("correct behavior", () => {
    test("length is 1", () => {
        expect(Temporal.PlainDate.prototype.equals).toHaveLength(1);
    });

    test("basic functionality", () => {
        const firstPlainDate = new Temporal.PlainDate(1, 1, 1, "gregory");
        const secondPlainDate = new Temporal.PlainDate(0, 1, 1, "gregory");
        expect(firstPlainDate.equals(firstPlainDate)).toBeTrue();
        expect(firstPlainDate.equals(secondPlainDate)).toBeFalse();
    });
});
