from platformio.test.runners.base import TestRunnerBase
from platformio.test.result import TestCase, TestStatus


class CustomTestRunner(TestRunnerBase):
    def on_testing_line_output(self, line):
        super().on_testing_line_output(line)
        if line.strip() == "All payload checks passed":
            self.test_suite.add_case(TestCase(name="payload checks", status=TestStatus.PASSED))
